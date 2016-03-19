var ipaddress = process.env.OPENSHIFT_NODEJS_IP || "127.0.0.1";
var port      = process.env.OPENSHIFT_NODEJS_PORT || 8080;

var WebSocketServer = require('ws').Server
var app = require('express')();
var http = require('http');
var server = http.createServer(app);

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

app.get('/emc_socket', function (req, res) {
    res.sendFile(__dirname + '/emc_socket/index.html');
});
app.get('/emc_socket/index.js', function (req, res) {
    res.sendFile(__dirname + '/emc_socket/index.js');
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

wss = new WebSocketServer({
    server: server,
    autoAcceptConnections: false
});

wss.broadcast = function broadcast(data, sender) {
    wss.clients.forEach(function each(client) {
        if (client != sender)
            client.send(data);
    });
};

wss.on('connection', function(ws) {
  console.log("New connection");
  ws.on('message', function (message) {
      //  if (message.type === 'utf8')
      console.log('received: %s', message);
      wss.broadcast(message, ws);
  });
  ws.send(str2ab('something'));
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
