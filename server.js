var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port      = process.env.OPENSHIFT_NODEJS_PORT || 8080;

var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
var server = http.createServer(app);
var sessionIDLen = 5,
    headerLen = 3 + sessionIDLen,
    clientIDLen = 5;
var sessions = [];
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
    var buf = new ArrayBuffer(str.length); // 2 bytes for each char
    var bufView = new Uint8Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
function generateID(count) {
    var symbols = '1234567890abcdefghijklmnopqrstuvwxyz',
        res = '';
    for (i = 0; i < count; ++i) {
        res += symbols[(Math.random() * symbols.length) | 0];
    }
    return ""+res;
}
function getSessionFromMsg(msg) {
    if (msg.length < 3 + sessionIDLen)
        return undefined
    if (msg[0] != 'N' || msg[1] != 'i' || msg[0] != 'N')
        return undefined;
    var sessionID = String.fromCharCode.apply(null, new Uint8Array(msg, 3, sessionIDLen));
    var session = sessions[sessionID];
    return session;
}
app.get('/emc_socket', function (req, res) {
    res.sendFile(__dirname + '/emc_socket/index.html');
});

app.get('/emc_socket/index.js', function (req, res) {
    res.sendFile(__dirname + '/emc_socket/index.js');
});

app.get('/', function (req, res) {
    if (req.query.p && sessions[req.query.p] != undefined) {
        res.sendFile(__dirname + '/emc_ogl/main.html');
        //  ?p=' + req.query.p);
    } else {
        var sessionID = generateID(sessionIDLen);
        console.log('starting new session: ' + sessionID)
        sessions[sessionID] = [];
        res.redirect('/?p=' + sessionID);
    }
});

app.get('/main.js', function (req, res) {
    res.sendFile(__dirname + '/emc_ogl/main.js');
});

/*app.use(function(req, res, next) {
    console.log("Sending compressed index.js");
  if (req.originalUrl === "/index.js") {
    req.url = "/emc/index.js.gz";
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/javascript');
  }
  next();
});*/

server.listen( port, ipaddress, function() {
    console.log((new Date()) + ' Server is listening on port 8080');
});

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
// emc_socket echo
//wss.broadcast = function broadcast(data, sender) {
//    wss.clients.forEach(function each(client) {
//        if (client != sender)
//            client.send(data);
//    });
//};

//wss.on('connection', function(ws) {
//  console.log("New connection");
//  ws.on('message', function (message) {
//      //  if (message.type === 'utf8')
//      console.log('received: %s', message);
//      wss.broadcast(message, ws);
//  });
//  ws.send(str2ab('something'));
//});

function broadcastToSession(sender, session, data) {
    session.forEach(function each(client) {
        if (client != sender)
            client.send(data);
    });
};
wss.on('connection', function(ws) {
    console.log("New connection");
    ws.on('message', function (message) {
        var session = getSessionFromMsg(msg);
        if (session == undefined) {
            ws.terminate();
            return;
        }
        session[ws] = ws;
        ws.session = session;
        //  if (message.type === 'utf8')
        console.log('received: %s', message);
        broadcastToSession(ws, session, message);
    });
    ws.on('close', function (code, message) {
        if (ws.session) {
            broadcastToSession(ws, ws.session, "KILL" + ws.clientID);
            if (ws.session.length > 1)
                ws.session[ws.session.indexOf(ws)] = ws.session[ws.session.length - 1];
            if (ws.session.length > 0)
                ws.session.pop();
        }
        console.log('Client ' + ws.clientID + ' disconnected ' + code + ' ' + message);
    });
    ws.clientID = generateID(clientIDLen);
    ws.send(str2ab("CONN" + ws.clientID));
});

/*function isAllowedOrigin(origin) {
    valid_origins = ['http://localhost', '127.0.0.1'];
    if (valid_origins.indexOf(origin) != -1) { // is in array
        console.log('Connection accepted from origin ' + origin);
        return true;
    }
    return false;
}
wss.on('request', function(request) {
  var connection = isAllowedOrigin(request.origin) ?
    request.accept() :
    request.reject();
}*/
console.log("Listening to " + ipaddress + ":" + port + "...");
