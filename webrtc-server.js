var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port = process.env.OPENSHIFT_NODEJS_PORT || 8080;
var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
var server = http.createServer(app);
var clients = [];
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
    for (var i = 0; i < clients.length; ++i) {
        if (clients[i] === sender) continue;
        console.log('>>>broadcast ' + i);
        try {
            clients[i].send(message);
        } catch (err) {
            console.log('client unexpectedly closed: ' + err.message);
        }
    }
}

wss.on('connection', function (ws) {
    clients.push(ws);
    ws.on('message', function (message, flags) {
        console.log('\n>>>onmessage' + message);
        //var data = JSON.parse(message);
        //if (data.type === "recv")
        broadcastMessage(ws, message)
    });
    ws.on('close', function (code, message) {
        console.log('Client disconnected ' + code + ' ' + message);
        clients.splice(clients.indexOf(ws), 1);
    });
});

server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port 8080');
});
