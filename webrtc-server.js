"use strict";
const Session_1 = require("./Session");
const Express = require('express');
const utils = require('./utils');
let http = require('http');
let ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
let port = process.env.OPENSHIFT_NODEJS_PORT || 8080;
let app = Express();
let server = http.createServer(app);
let debug = new utils.Debug(false, false);
let rootPath = ''; //'/develop';
let sessionIDLen = 5, clientIDLen = 5, minPlayers = 4, maxPlayers = 16, maxSessions = 8;
let RTCClients = new Map();
let sessions = new Map();
// session handling
function getSessionIDFromMsg(msg) {
    if (msg.length < 4 + sessionIDLen)
        return undefined;
    return msg.substr(4, sessionIDLen);
}
function getClientIDFromMsg(msg) {
    if (msg.length < 4 + clientIDLen)
        return undefined;
    return msg.substr(4, clientIDLen);
}
function findAvailableSessionID() {
    let res = null;
    for (var session of sessions) {
        if (!res || session[1].clients.length < res[1].clients.length)
            res = session;
    }
    return (res && res[1].clients.length < maxPlayers) ? res[0] : undefined;
}
function redirectToASession(res) {
    let id;
    if (id = findAvailableSessionID()) {
        console.log('found an existing session: ' + id);
        res.redirect(rootPath + '/?p=' + id);
    }
    else if (sessions.size >= maxSessions) {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('All sessions are full. Try again later.');
    }
    else {
        function generateID(count) {
            var symbols = '1234567890abcdefghijklmnopqrstuvwxyz', res = '';
            for (var i = 0; i < count; ++i)
                res += symbols[(Math.random() * symbols.length) | 0];
            return "" + res;
        }
        var sessionID = generateID(sessionIDLen);
        console.log('starting new session: ' + sessionID);
        let session = new Session_1.Session();
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
    let session = null;
    if (req.query.p &&
        (session = sessions.get(req.query.p)) != undefined &&
        session.clients.length < maxPlayers) {
        res.sendFile(__dirname + '/emc_ogl/main.html'); //  ?p=' + req.query.p);
    }
    else {
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
function applyMixins(derivedCtor, baseCtors) {
    baseCtors.forEach(baseCtor => {
        Object.getOwnPropertyNames(baseCtor.prototype).forEach(name => {
            derivedCtor.prototype[name] = baseCtor.prototype[name];
        });
    });
}
function handleSessionStringMessage(client, message) {
    if (message.indexOf('SESS') === 0) {
        var sessionID = getSessionIDFromMsg(message);
        if (!sessionID) {
            client.close();
            return;
        }
        var session = sessions.get(sessionID);
        if (!session) {
            client.close();
            return;
        }
        let clientToCtrl = session.findClientToCtrl();
        if (clientToCtrl) {
            clientToCtrl.ctrl = 1;
            clientToCtrl.otherId = client.id;
            clientToCtrl.sendSessionStringMessage('CTRL' + clientToCtrl.ctrl);
            client.ctrl = 2;
            client.otherId = clientToCtrl.id;
            debug.Log(client.id + ' ' + clientToCtrl.id);
        }
        client.sendSessionStringMessage('CONN' + (clientToCtrl ? clientToCtrl.id : client.id) + client.ctrl + client.id);
        client.session = session;
        session.clients.push(client);
        if (session.clients.length < minPlayers)
            session.broadcastStringToSession(null, 'WAIT' + (minPlayers - session.clients.length));
        else
            session.broadcastStringToSession(null, 'WAIT0');
        return;
    }
    else if (message.indexOf('KILL') === 0) {
        debug.Log("killing " + message);
        var clientIDToKill = getClientIDFromMsg(message);
        if (clientIDToKill) {
            var clientToKill = client.session.findClientByID(clientIDToKill);
            if (clientToKill)
                clientToKill.close();
        }
    }
}
function close(client) {
    for (var [remoteID, ws] of RTCClients.entries()) {
        if (ws === client.ws) {
            delete RTCClients.delete(remoteID);
            client.session.broadcastToSession(client, JSON.stringify({ 'close': remoteID }));
            break;
        }
    }
    if (client.session) {
        let session = client.session;
        session.broadcastStringToSession(client, 'KILL' + client.id);
        debug.Log((new Date()) + ">>>>>KILL + " + client.id);
        session.removeClient(client);
        // check for other client to reset control
        if (client.otherId) {
            session.broadcastStringToSession(client, 'KILL' + client.otherId);
            debug.Log((new Date()) + ">>>>>KILL + " + client.otherId);
            let clientToResetCtrl;
            if (clientToResetCtrl = session.findClientByID(client.otherId)) {
                clientToResetCtrl.ctrl = 0;
            }
        }
        if (session.clients.length < 1) {
            delete sessions.delete(session.id);
            debug.Log('deleting session: ' + session.id + ' session count: ' + sessions.size);
        }
        else if (session.clients.length < minPlayers)
            session.broadcastStringToSession(null, 'WAIT' + (minPlayers - session.clients.length));
    }
    //debugOut('Client disconnected, count ' + clients.size + ' ' + code + ' ' + message);
}
// {'connect': '7fea5'}
// {'offer': {'originID': '7fea5', 'targetID': '8e9c3', 'sdp': '...'}}
// {'answer': {'targetID': '7fea5','sdp': '...'}}
// {'targetID': '8e9c3', 'originID': originID, 'candidate': '...'}}
let wss;
if (ipaddress === "127.0.0.1") {
    wss = new Session_1.WS.Server({
        server: server,
        port: 8000 });
}
else {
    wss = new Session_1.WS.Server({
        server: server
    });
}
/*let pingId = setInterval(function(){
    let clientsToClose : Client[] = [];
    for (var [sessionID, session] of sessions) {
        for (var client of session.clients) {
            if (!client.alive) {
               // debug.Log("ToClose " + client.id);
                clientsToClose.push(client);
            }
            else {
                //debug.Log("wasalive " + client.id);
                client.alive = false;
            }
        }
       // debug.Log("session " + session.clients.length);
        session.broadcastToSession(null, JSON.stringify({'ping': 'ping'}));
    }
    for (var client of clientsToClose) {
        debug.Log('Closing ' + client.id);
        close(client);
    }
}, 1000);*/
wss.on('connection', function (ws) {
    let client = new Session_1.Client(ws);
    ws.on('message', function (message, flags) {
        var msg = JSON.parse(message);
        if (msg.session) {
            handleSessionStringMessage(client, msg.session);
        }
        else if (msg.ping) {
            client.alive = true;
        }
        else if (msg.connect) {
            // broadcast connect request to everyone else in the session
            client.session.broadcastToSession(client, message);
            RTCClients.set(msg.connect, ws);
        }
        else if (msg.offer) {
            debug.LogRTC(">>>offer from " + msg.offer.originID);
            var rtcClient = RTCClients.get(msg.offer.targetID);
            if (rtcClient) {
                rtcClient.send(message);
                debug.LogRTC(">>>offer sent to " + msg.offer.targetID);
            }
        }
        else if (msg.answer) {
            debug.LogRTC(">>>answer from " + msg.answer.originID);
            var rtcClient = RTCClients.get(msg.answer.targetID);
            if (rtcClient) {
                rtcClient.send(message);
                debug.LogRTC(">>>answer sent to" + msg.answer.targetID);
            }
        }
        else if (msg.candidate) {
            debug.LogRTC(">>>icecandidate from " + msg.originID);
            var rtcClient = RTCClients.get(msg.targetID);
            if (rtcClient) {
                rtcClient.send(message);
                debug.LogRTC(">>>icecandidate sent to " + msg.targetID);
            }
        }
    });
    ws.on('close', function (code, message) {
        close(client);
    });
});
server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port ' + port);
});
