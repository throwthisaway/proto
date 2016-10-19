var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port = process.env.OPENSHIFT_NODEJS_PORT || 8080;
var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
var server = http.createServer(app);
var clients = new Map();
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
function broadcastMessage(sender, message) {
    for (var client of clients.values()) {
        if (client === sender) continue;
        try {
            client.send(message);
        } catch (err) {
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
            console.log(">>>>>offer from " + msg.offer.originID);
            var client = clients.get(msg.offer.targetID);
            if (client) {
                client.send(message);
                console.log(">>>>>offer sent to " + msg.offer.targetID);
            }
        }else if (msg.answer) {
            console.log(">>>>>answer from " + msg.answer.originID);
            var client = clients.get(msg.answer.targetID);
            if (client) {
                client.send(message);
                console.log(">>>>>answer sent to" + msg.answer.targetID);
            }
        }else if (msg.candidate) {
            console.log(">>>>>icecandidate from " + msg.originID);
            var client = clients.get(msg.targetID);
            if (client) {
                client.send(message);
                console.log(">>>>>icecandidate sent to " + msg.targetID);
            }
        }
    });
    ws.on('close', function (code, message) {
        console.log('Client disconnected ' + code + ' ' + message);
        for (var [key, value] of clients.entries()) {
            if (value === ws) {
                clients.delete(key);
                break;
            }
        }
        console.log('Client count ' + clients.size);
    });
});

server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port 8080');
});
