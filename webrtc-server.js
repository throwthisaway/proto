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

var wss;
if (ipaddress === "127.0.0.1") {
    wss = new WebSocketServer({
        server: server,
        //port: 8000,
        autoAcceptConnections: false
    });
} else {
    wss = new WebSocketServer({
        server: server,
        autoAcceptConnections: false
    });
}
wss.on('connection', function (ws) {
    clients.push(ws);
    ws.on('message', function (message, flags) {
        for (var i = 0; i < clients.length; ++i) {
            if (clients[i] == ws) continue;
            ws.send(message);
        }
    });
    ws.on('close', function (code, message) {
        console.log('Client disconnected ' + code + ' ' + message);
    });
});

server.listen(port, ipaddress, function () {
    console.log((new Date()) + ' Server is listening on port 8080');
});
