var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port      = process.env.OPENSHIFT_NODEJS_PORT || 8080;
var release = '';//'/develop';
var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
//var redis = require('redis');
var server = http.createServer(app);
//var dispatcher = redis.createClient();
//var subscriber = redis.createClient(); 

//subscriber.subscribe('dispatch');
//subscriber.on('message', function (channel, data) {

//    console.log('>>>>>%s', data);
//    //for (var i = 0; i < clients.length; ++i) {
//        //clients[i].write(msg);
//    //}
//});

var sessionIDLen = 5,
    headerLen = 3 + sessionIDLen,
    clientIDLen = 5,
    minPlayers = 4,
    maxPlayers = 16,
    maxSessions = 8;
var sessions = new Map();
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
    var sessionID = String.fromCharCode.apply(null, msg);
    console.log('session init, id: %s', sessionID);
    return sessionID.substr(4, sessionIDLen);
}
//app.get('/emc_socket', function (req, res) {
//    res.sendFile(__dirname + '/emc_socket/index.html');
//});

//app.get('/emc_socket/index.js', function (req, res) {
//    res.sendFile(__dirname + '/emc_socket/index.js');
//});

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

app.get(release, function (req, res) {
    var session;
    if (req.query.p &&
        (session = sessions.get(req.query.p)) != undefined &&
        session.length < maxPlayers) {
        res.sendFile(__dirname + '/emc_ogl/main.html');//  ?p=' + req.query.p);
    } else redirectToASession(res);
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

//app.get('/', function (req, res) {
//    console.log('coming_soon.html visit: ' + new Date().toLocaleString());
//    res.sendFile(__dirname + '/coming_soon.html');
//});
//app.get('/coming_soon.png', function (req, res) {
//    res.sendFile(__dirname + '/coming_soon.png');
//});

app.get('/thumbnail4.png', function (req, res) {
    res.sendFile(__dirname + '/thumbnail4.png');
});

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
wss.on('connection', function(ws) {
    //console.log("New connection");
    ws.on('message', function (message, flags) {
        //console.log('received: %s', message);
        if (message.length >= 4 && String.fromCharCode(message[0], message[1], message[2], message[3]) === 'SESS') {
            var sessionID = getSessionIDFromMsg(message);
            //dispatcher.publish('dispatch',  message );
            var session = sessions.get(sessionID);
            if (session == undefined) {
                ws.terminate();
                return;
            }
            var client = findClientToCtrl(session);
            if (client) {
                client.ctrl = 1;
                client.send(str_and_number2ab('CTRL', 1));
                ws.ctrl = 2;
                ws.clientID = client.clientID;
                ws.send(str2ab("CONN" + client.clientID + "2"));
            } else {
                ws.ctrl = 0;
                ws.clientID = generate0ToOClientID(clientIDLen);
                ws.send(str2ab("CONN" + ws.clientID + "0"));
            }
            session.push(ws);
            ws.session = session;
            console.log('session player count: ' + session.length);

            if (session.length < minPlayers)
                broadcastToSession(null, session, str_and_number2ab('WAIT', minPlayers - session.length));
            else
                broadcastToSession(null, session, str_and_number2ab('WAIT', 0));
            return;
        }
        if (ws.session == undefined) {
            console.log('Invalid session for ws client');
            ws.terminate();
            return;
        }
        //  if (message.type === 'utf8')
        broadcastToSession(ws, ws.session, message);
    });
    ws.on('close', function (code, message) {
        if (ws.session) {
            broadcastToSession(ws, ws.session, str2ab('KILL' + ws.clientID));
            if (ws.session.length > 1)
                ws.session[ws.session.indexOf(ws)] = ws.session[ws.session.length - 1];
            ws.session.pop();
            var client;
            if (client = findClientByID(ws.session, ws.clientID)) {
                client.ctrl = 0;
                client.send(str_and_number2ab('CTRL', 0));
            }
            console.log('session player count: ' + ws.session.length);
            if (ws.session.length < 1) {
                console.log('deleting session: ' + ws.session.id );
                delete sessions.delete(ws.session.id);
                console.log(' session count: ' + sessions.size);
            } else  if (ws.session.length < minPlayers)
                broadcastToSession(null, ws.session, str_and_number2ab('WAIT', minPlayers - ws.session.length));
        }
        console.log('Client ' + ws.clientID + ' disconnected ' + code + ' ' + message);
    });
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
