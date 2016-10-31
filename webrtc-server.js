var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port = process.env.OPENSHIFT_NODEJS_PORT || 8080;
var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
var server = http.createServer(app);

var debug = true, debugRTC = false;
var release = '';//'/develop';
var sessionIDLen = 5,
    headerLen = 3 + sessionIDLen,
    clientIDLen = 5,
    minPlayers = 4,
    maxPlayers = 16,
    maxSessions = 8;
var clients = new Map();
var sessions = new Map();

function debugOutRTC(msg) {
    debugRTC && debugOut(msg);
}
function debugOut(msg) {
    debug && console.log(msg);
}
//app.get('/', function (req, res) {
//    res.sendFile(__dirname + '/webrtc/webrtc.html');
//});
//app.get('/webrtcpeer.js', function (req, res) {
//    res.sendFile(__dirname + '/webrtc/webrtcpeer.js');
//});

//app.get('/adapter.js', function (req, res) {
//    res.sendFile(__dirname + '/webrtc/adapter.js');
//});
function ab2strUtf16(buf) {
    return String.fromCharCode.apply(null, new Uint16Array(buf));
}
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
function str2abUtf16(str) {
    var buf = new ArrayBuffer(str.length * 2); // 2 bytes for each char
    var bufView = new Uint16Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
function str2ab(str) {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
function str_and_number2ab(str, num) {
    var numLen = 1; // 1 byte number size, 3 byte padding
    var buf = new ArrayBuffer(str.length + numLen);
    var bufView = new Uint8Array(buf);
    for (var i = 0,strLen = str.length; i < strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    bufView[bufView.length - 1] = num;
    return buf;
}
function generateID(count) {
    var symbols = '1234567890abcdefghijklmnopqrstuvwxyz',
        res = '';
    for (var i = 0; i < count; ++i) {
        res += symbols[(Math.random() * symbols.length) | 0];
    }
    return "" + res;
}
function generate0ToOClientID(count) {
    //var symbols = '0123456789:;<=>?@ABCDEFGHIJKLMNO',
    var res = '';
    for (var i = 0; i < count; ++i) {
        res += String.fromCharCode((48 + Math.random() * 32) | 0);
    }
    return ""+res;
}
function getSessionIDFromMsg(msg) {
    if (msg.length < 4 + sessionIDLen)
        return undefined
    console.log('session init, id: %s', msg);
    return msg.substr(4, sessionIDLen);
}
function findAvailableSessionID() {
    var res;
    for (var session of sessions) {
        if (!res || session[1].length<res[1].length)
            res = session;
    }
    return (res && res[1].length < maxPlayers) ? res[0] : undefined;
}
function redirectToASession(res) {
    var id;
    if (id = findAvailableSessionID()) {
        console.log('found an existing session: ' + id);
        res.redirect(release + '/?p=' + id);
    } else if (sessions.size >= maxSessions) {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('All sessions are full. Try again later.')
    } else {
        var sessionID = generateID(sessionIDLen);
        console.log('starting new session: ' + sessionID);
        var session = [];
        session.id = sessionID;
        sessions.set(sessionID, session);
        res.redirect(release + '/?p=' + sessionID);
    }
}

app.get('/webrtc/webrtcpeer.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/webrtcpeer.js');
});

app.get('/webrtc/adapter.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/adapter.js');
});

app.get(release, function (req, res) {
    var session;
    if (req.query.p &&
        (session = sessions.get(req.query.p)) != undefined &&
        session.length < maxPlayers) {
        res.sendFile(__dirname + '/emc_ogl/main.html');//  ?p=' + req.query.p);
    } else {
        debugOut(req.headers);
        redirectToASession(res);
    }
});

app.get(release + '/main.js', function (req, res) {
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/javascript');
    res.sendFile(__dirname + '/emc_ogl/main.js.gz');
});
app.get(release + '/main.js.mem', function (req, res) {
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/octet-stream');
    res.sendFile(__dirname + '/emc_ogl/main.js.mem.gz');
});
app.get(release + '/main.data', function (req, res) {
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/octet-stream');
    res.sendFile(__dirname + '/emc_ogl/main.data.gz');
});
app.get('/thumbnail4.png', function (req, res) {
    res.sendFile(__dirname + '/thumbnail4.png');
});

function broadcastStringToSession(sender, session, data) {
    session.forEach(function each(client) {
        try{
            if (client != sender)
                sendSessionStringMessage(client, data);
        } catch(err) {
            console.log('client unexpectedly closed: ' + err.message); }
    });
};
function broadcastToSession(sender, session, data) {
    session.forEach(function each(client) {
        try{
            if (client != sender)
                client.send(data);
        } catch(err) {
            console.log('client unexpectedly closed: ' + err.message); }
    });
};
function findClientToCtrl(session) {
    for (var i = 0; i < session.length; ++i) {
        //console.log('findClient: ' + session[i].ctrl);
        if (session[i].ctrl === 0) return session[i];
    }
    //console.log('findClient: not found ');
    return null;
}
function findClientByID(session, id) {
    for (var i = 0; i < session.length; ++i) {
        // console.log('findClientbyid: ' + session[i].clientID + " " + id);
        if (session[i].clientID === id) return session[i];
    }
    //console.log('findClientbyid: not found ');
    return null;
}
function sendSessionStringMessage(ws, str){
    ws.send(JSON.stringify({'session': str}));
}
function handleSessionStringMessage(ws, message){
    debugOut(message);
    if (message.indexOf('SESS') === 0) {
        var sessionID = getSessionIDFromMsg(message);
        var session = sessions.get(sessionID);
        if (session == undefined) {
            debugOut('Can\'t find session ' + sessionID);
            ws.terminate();
            return;
        }
        var client = findClientToCtrl(session);
        if (client) {
            client.ctrl = 1;
            sendSessionStringMessage(client, 'CTRL1');
            ws.ctrl = 2;
            ws.clientID = client.clientID;
            sendSessionStringMessage(ws, "CONN" + client.clientID + "2");
        } else {
            ws.ctrl = 0;
            ws.clientID = generate0ToOClientID(clientIDLen);
            sendSessionStringMessage(ws, "CONN" + ws.clientID + "0");
        }
        session.push(ws);
        ws.session = session;
        console.log('session player count: ' + session.length);

        if (session.length < minPlayers)
            broadcastStringToSession(null, session, 'WAIT' + ab2str([48 + minPlayers - session.length]));
        else
            broadcastStringToSession(null, session, 'WAIT0');
        return;
    }
    if (ws.session == undefined) {
        console.log('Invalid session for ws client');
        ws.terminate();
        return;
    }
    //broadcastToSession(ws, ws.session, message);
}

function close(ws, remoteID){
    //debugOut("closing " + remoteID)
    clients.delete(remoteID);
    broadcastToSession(ws, ws.session, JSON.stringify({'close': remoteID}));
}

//function broadcastMessage(sender, message) {
//    for (var [key, client] of clients.entries()) {
//        if (client === sender) continue;
//        try {
//            client.send(message);
//        } catch (err) {
//            close(sender, key);
//            console.log('client unexpectedly closed: ' + err.message);
//        }
//    }
//}

// {'connect': '7fea5'}
// {'offer': {'originID': '7fea5', 'targetID': '8e9c3', 'sdp': '...'}}
// {'answer': {'targetID': '7fea5','sdp': '...'}}
// {'targetID': '8e9c3', 'originID': originID, 'candidate': '...'}}
var wss;
if (ipaddress === "127.0.0.1") {
    wss = new WebSocketServer({
        server: server,
        port: 8000,
        autoAcceptConnections: false });
} else {
    wss = new WebSocketServer({
        server: server,
        autoAcceptConnections: false
    });
}
wss.on('connection', function (ws) {
    ws.on('message', function (message, flags) {
        var msg = JSON.parse(message);
        if (msg.session){
            handleSessionStringMessage(ws, msg.session);
        } else if (msg.connect) {
            // broadcast connect request to everyone else in the session
            broadcastToSession(ws, ws.session, message);
            clients.set(msg.connect, ws);
        }else if (msg.offer) {
            debugOutRTC(">>>offer from " + msg.offer.originID);
            var client = clients.get(msg.offer.targetID);
            if (client) {
                client.send(message);
                debugOutRTC(">>>offer sent to " + msg.offer.targetID);
            }
        }else if (msg.answer) {
            debugOutRTC(">>>answer from " + msg.answer.originID);
            var client = clients.get(msg.answer.targetID);
            if (client) {
                client.send(message);
                debugOutRTC(">>>answer sent to" + msg.answer.targetID);
            }
        }else if (msg.candidate) {
            debugOutRTC(">>>icecandidate from " + msg.originID);
            var client = clients.get(msg.targetID);
            if (client) {
                client.send(message);
                debugOutRTC(">>>icecandidate sent to " + msg.targetID);
            }
        }
    });
    ws.on('close', function (code, message) {
        if (ws.session) {
            if (ws.ctrl == 0)
                broadcastStringToSession(ws, ws.session, 'KILL' + ws.clientID);
            if (ws.session.length > 1)
                ws.session[ws.session.indexOf(ws)] = ws.session[ws.session.length - 1];
            ws.session.pop();
            // check for other client to reset control
            var client;
            if (client = findClientByID(ws.session, ws.clientID)) {
                client.ctrl = 0;
                sendSessionStringMessage(client, 'CTRL0');
            }
            if (ws.session.length < 1) {
                delete sessions.delete(ws.session.id);
               // debugOut('deleting session: ' + ws.session.id + ' session count: ' + sessions.size);
            } else  if (ws.session.length < minPlayers)
                broadcastStringToSession(null, ws.session, 'WAIT' + ab2str([48+minPlayers - ws.session.length]));
        }
        //debugOut('Client ' + ws.clientID + ' disconnected ' + code + ' ' + message);

        for (var [key, value] of clients.entries()) {
             if (value === ws) {
                 close(ws, key);
                 break;
             }
        }
        //debugOut('Client disconnected, count ' + clients.size + ' ' + code + ' ' + message);
    });
});

server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port 8080');
});
