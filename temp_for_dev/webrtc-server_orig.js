var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port = process.env.OPENSHIFT_NODEJS_PORT || 8080;
var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
var server = http.createServer(app);
var clients = new Map();
var debug = true;
function debugOut(msg) {
    if (debug)
        console.log(msg);
}
app.get('/', function (req, res) {
    res.sendFile(__dirname + '/webrtc/webrtc.html');
});
app.get('/webrtcpeer.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/webrtcpeer.js');
});

app.get('/adapter.js', function (req, res) {
    res.sendFile(__dirname + '/webrtc/adapter.js');
});

function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}

var wss = new WebSocketServer({
    server: server,
    //port: 8000,
    autoAcceptConnections: false
});

function close(ws, remoteID){
    debugOut("closing " + remoteID)
    clients.delete(remoteID);
    broadcastMessage(ws, JSON.stringify({'close': remoteID}));
}

function broadcastMessage(sender, message) {
    for (var [key, client] of clients.entries()) {
        if (client === sender) continue;
        try {
            client.send(message);
        } catch (err) {
            close(sender, key);
            console.log('client unexpectedly closed: ' + err.message);
        }
    }
}
// {'connect': '7fea5'}
// {'offer': {'originID': '7fea5', 'targetID': '8e9c3', 'sdp': '...'}}
// {'answer': {'targetID': '7fea5','sdp': '...'}}

// {'targetID': '8e9c3', 'originID': originID, 'candidate': '...'}}
wss.on('connection', function (ws) {
    ws.on('message', function (message, flags) {
        var msg = JSON.parse(message);
        if (msg.connect) {
            // broadcast connect request to everyone else
            broadcastMessage(ws, message)
            clients.set(msg.connect, ws);
        }else if (msg.offer) {
            debugOut(">>>>>offer from " + msg.offer.originID);
            var client = clients.get(msg.offer.targetID);
            if (client) {
                client.send(message);
                debugOut(">>>>>offer sent to " + msg.offer.targetID);
            }
        }else if (msg.answer) {
            debugOut(">>>>>answer from " + msg.answer.originID);
            var client = clients.get(msg.answer.targetID);
            if (client) {
                client.send(message);
                debugOut(">>>>>answer sent to" + msg.answer.targetID);
            }
        }else if (msg.candidate) {
            debugOut(">>>>>icecandidate from " + msg.originID);
            var client = clients.get(msg.targetID);
            if (client) {
                client.send(message);
                debugOut(">>>>>icecandidate sent to " + msg.targetID);
            }
        }
    });
    ws.on('close', function (code, message) {
        for (var [key, value] of clients.entries()) {
            if (value === ws) {
                debugOut("delete " + key);
                close(ws, key);
                break;
        }
        debugOut('Client disconnected, count ' + clients.size + ' ' + code + ' ' + message);
    }
     
    });
});

server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port 8080');
});
