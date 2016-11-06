"use strict";
var Session_1 = require("./Session");
var Express = require('express');
var utils = require('./utils');
var http = require('http');
var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port = process.env.OPENSHIFT_NODEJS_PORT || 8080;
var app = Express();
var server = http.createServer(app);
var debug = new utils.Debug(true, false);
var rootPath = ''; //'/develop';
var sessionIDLen = 5, minPlayers = 4, maxPlayers = 16, maxSessions = 8;
var RTCClients = new Map();
var sessions = new Map();
// session handling
function getSessionIDFromMsg(msg) {
    if (msg.length < 4 + sessionIDLen)
        return undefined;
    return msg.substr(4, sessionIDLen);
}
function findAvailableSessionID() {
    var res = null;
    for (var _i = 0, sessions_1 = sessions; _i < sessions_1.length; _i++) {
        var session = sessions_1[_i];
        if (!res || session[1].clients.length < res[1].clients.length)
            res = session;
    }
    return (res && res[1].clients.length < maxPlayers) ? res[0] : undefined;
}
function redirectToASession(res) {
    var id;
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
        var session = new Session_1.Session();
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
    var session = null;
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
    baseCtors.forEach(function (baseCtor) {
        Object.getOwnPropertyNames(baseCtor.prototype).forEach(function (name) {
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
        var clientToCtrl = session.findClientToCtrl();
        if (clientToCtrl) {
            clientToCtrl.ctrl = 1;
            clientToCtrl.otherId = client.id;
            clientToCtrl.sendSessionStringMessage('CTRL' + clientToCtrl.ctrl);
            client.ctrl = 2;
            client.otherId = clientToCtrl.id;
            console.log(client.id + ' ' + clientToCtrl.id);
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
    if (client.session == undefined) {
        console.log('Invalid session for ws client');
        client.close();
        return;
    }
}
function close(client, remoteID) {
    RTCClients.delete(remoteID);
    client.session.broadcastToSession(client, JSON.stringify({ 'close': remoteID }));
}
// {'connect': '7fea5'}
// {'offer': {'originID': '7fea5', 'targetID': '8e9c3', 'sdp': '...'}}
// {'answer': {'targetID': '7fea5','sdp': '...'}}
// {'targetID': '8e9c3', 'originID': originID, 'candidate': '...'}}
var wss;
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
wss.on('connection', function (ws) {
    var client = new Session_1.Client(ws);
    ws.on('message', function (message, flags) {
        var msg = JSON.parse(message);
        if (msg.session) {
            handleSessionStringMessage(client, msg.session);
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
        for (var _i = 0, _a = RTCClients.entries(); _i < _a.length; _i++) {
            var _b = _a[_i], key = _b[0], value = _b[1];
            if (value === ws) {
                close(client, key);
                break;
            }
        }
        if (client.session) {
            var session = client.session;
            session.broadcastStringToSession(client, 'KILL' + client.id);
            session.removeClient(client);
            // check for other client to reset control
            if (client.otherId) {
                var clientToResetCtrl = void 0;
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
    });
});
server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port ' + port);
});
