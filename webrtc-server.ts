import {Client, Session, WS} from "./Session"
import * as Express from 'express';
import * as utils from './utils'
let http = require('http');
let ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
let port : number|any = process.env.OPENSHIFT_NODEJS_PORT || 8080;
let app = Express();
let server = http.createServer(app);
let debug = new utils.Debug(true, false);
let rootPath = '';//'/develop';
let sessionIDLen = 5,
    clientIDLen = 5,
    minPlayers = 4,
    maxPlayers = 16,
    maxSessions = 8;
let RTCClients = new Map<string, WS>();
let sessions = new Map<string, Session>();

// session handling
function getSessionIDFromMsg(msg : string) : string | undefined {
    if (msg.length < 4 + sessionIDLen)
        return undefined
    console.log('session init, id: %s', msg);
    return msg.substr(4, sessionIDLen);
}
function findAvailableSessionID() : string | undefined{
    let res : [string, Session] | null = null;
    for (var session of sessions) {
        if (!res || session[1].clients.length<res[1].clients.length)
            res = session;
    }
    return (res && res[1].clients.length < maxPlayers) ? res[0] : undefined;
}
function redirectToASession(res : Express.Response) {
    let id : string | undefined;
    if (id = findAvailableSessionID()) {
        console.log('found an existing session: ' + id);
        res.redirect(rootPath + '/?p=' + id);
    } else if (sessions.size >= maxSessions) {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('All sessions are full. Try again later.')
    } else {
        function generateID(count: number): string {
            var symbols = '1234567890abcdefghijklmnopqrstuvwxyz', res = '';
            for (var i = 0; i < count; ++i)
                res += symbols[(Math.random() * symbols.length) | 0];
            return "" + res;
        }
        var sessionID = generateID(sessionIDLen);
        console.log('starting new session: ' + sessionID);
        let session = new Session();
        session.id = sessionID;
        sessions.set(sessionID, session);
        res.redirect(rootPath + '/?p=' + sessionID);
    }
}

// page serve
app.get('/webrtc/utils.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/utils.js');
});

app.get('/webrtc/webrtcpeer.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/webrtcpeer.js');
});

app.get('/webrtc/adapter.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/adapter.js');
});

app.get(rootPath, function (req, res) {
    let session : Session | null = null;
    if (req.query.p &&
        (session = sessions.get(req.query.p)) != undefined &&
        session.clients.length < maxPlayers) {
        res.sendFile(__dirname + '/emc_ogl/main.html');//  ?p=' + req.query.p);
    } else {
        debug.Log(req.headers);
        redirectToASession(res);
    }
});

app.get(rootPath + '/main.js', function (req, res) {
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/javascript');
    res.sendFile(__dirname + '/emc_ogl/main.js.gz');
});
app.get(rootPath + '/main.js.mem', function (req, res) {
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/octet-stream');
    res.sendFile(__dirname + '/emc_ogl/main.js.mem.gz');
});
app.get(rootPath + '/main.data', function (req, res) {
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/octet-stream');
    res.sendFile(__dirname + '/emc_ogl/main.data.gz');
});
app.get('/thumbnail4.png', function (req, res) {
    res.sendFile(__dirname + '/thumbnail4.png');
});

function applyMixins(derivedCtor: any, baseCtors: any[]) {
    baseCtors.forEach(baseCtor => {
        Object.getOwnPropertyNames(baseCtor.prototype).forEach(name => {
            derivedCtor.prototype[name] = baseCtor.prototype[name];
        });
    });
}
function handleSessionStringMessage(wsClient : Client, message : string){
    if (message.indexOf('SESS') === 0) {
        var sessionID = getSessionIDFromMsg(message);
        if (!sessionID) { wsClient.close(); return; }
        var session = sessions.get(sessionID);
        if (!session) { wsClient.close(); return; }
        let client = session.findClientToCtrl();
        if (client) {
            client.ctrl = 1;
            client.sendSessionStringMessage('CTRL1');
            wsClient.ctrl = 2;
            wsClient.clientID = client.clientID;
            wsClient.sendSessionStringMessage("CONN" + client.clientID + "2");
        } else {
            wsClient.ctrl = 0;
            wsClient.clientID = utils.generate0ToOClientID(clientIDLen);
            wsClient.sendSessionStringMessage("CONN" + wsClient.clientID + "0");
        }
        wsClient.session = session;
        session.clients.push(wsClient);
        if (session.clients.length < minPlayers)
            session.broadcastStringToSession(null, 'WAIT' + utils.ab2str([48 + minPlayers - session.clients.length]));
        else
            session.broadcastStringToSession(null, 'WAIT0');
        return;
    }
    if (wsClient.session == undefined) {
        console.log('Invalid session for ws client');
        wsClient.close();
        return;
    }
}

function close(client : Client, remoteID : string){
    RTCClients.delete(remoteID);
    client.session.broadcastToSession(client, JSON.stringify({'close': remoteID}));
}

// {'connect': '7fea5'}
// {'offer': {'originID': '7fea5', 'targetID': '8e9c3', 'sdp': '...'}}
// {'answer': {'targetID': '7fea5','sdp': '...'}}
// {'targetID': '8e9c3', 'originID': originID, 'candidate': '...'}}
let wss : WS.Server;
if (ipaddress === "127.0.0.1") {
    wss = new WS.Server({
        server: server,
        port: 8000 });
} else {
    wss = new WS.Server({
        server: server
    });
}
wss.on('connection', function (ws) {
    let client = new Client(ws);
    ws.on('message', function (message, flags) {
        var msg = JSON.parse(message);
        if (msg.session){
            handleSessionStringMessage(client, msg.session);
        } else if (msg.connect) {
            // broadcast connect request to everyone else in the session
            client.session.broadcastToSession(client, message);
            RTCClients.set(msg.connect, ws);
        }else if (msg.offer) {
            debug.LogRTC(">>>offer from " + msg.offer.originID);
            var rtcClient = RTCClients.get(msg.offer.targetID);
            if (rtcClient) {
                rtcClient.send(message);
                debug.LogRTC(">>>offer sent to " + msg.offer.targetID);
            }
        }else if (msg.answer) {
            debug.LogRTC(">>>answer from " + msg.answer.originID);
            var rtcClient = RTCClients.get(msg.answer.targetID);
            if (rtcClient) {
                rtcClient.send(message);
                debug.LogRTC(">>>answer sent to" + msg.answer.targetID);
            }
        }else if (msg.candidate) {
            debug.LogRTC(">>>icecandidate from " + msg.originID);
            var rtcClient = RTCClients.get(msg.targetID);
            if (rtcClient) {
                rtcClient.send(message);
                debug.LogRTC(">>>icecandidate sent to " + msg.targetID);
            }
        }
    });
    ws.on('close', function (code, message) {
        if (client.session) {
            let session = client.session;
            if (client.ctrl == 0)
                session.broadcastStringToSession(client, 'KILL' + client.clientID);
            session.removeClient(client);
            // check for other client to reset control
            let clientToResetCtrl : Client | null;
            if (clientToResetCtrl = session.findClientByID(client.clientID)) {
                clientToResetCtrl.ctrl = 0;
                clientToResetCtrl.sendSessionStringMessage('CTRL0');
            }
            if (session.clients.length < 1) {
                sessions.delete(session.id);
                debug.Log('deleting session: ' + session.id + ' session count: ' + sessions.size);
            } else  if (session.clients.length < minPlayers)
                session.broadcastStringToSession(null,'WAIT' + utils.ab2str([48+minPlayers - session.clients.length]));
        }

        for (var [key, value] of RTCClients.entries()) {
             if (value === ws) {
                 close(client, key);
                 break;
             }
        }
        //debugOut('Client disconnected, count ' + clients.size + ' ' + code + ' ' + message);
    });
});

server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port ' + port);
});
