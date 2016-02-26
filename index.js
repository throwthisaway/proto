var app = require('express')();
var http = require('http').Server(app);
var WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({ port: 8080 }); 
function ab2strUtf16(buf) {
  return String.fromCharCode.apply(null, new Uint16Array(buf));
}
function ab2str(buf) {
  return String.fromCharCode.apply(null, new Uint8Array(buf));
}
function str2abUtf16(str) {
  var buf = new ArrayBuffer(str.length*2); // 2 bytes for each char
  var bufView = new Uint16Array(buf);
  for (var i=0, strLen=str.length; i<strLen; i++) {
    bufView[i] = str.charCodeAt(i);
  }
  return buf;
}
function str2ab(str) {
  var buf = new ArrayBuffer(str.length); // 2 bytes for each char
  var bufView = new Uint8Array(buf);
  for (var i=0, strLen=str.length; i<strLen; i++) {
    bufView[i] = str.charCodeAt(i);
  }
  return buf;
}
app.get('/', function(req, res){
  res.sendFile(__dirname + '/emc_socket/index.html');
});
app.get('/index.js', function(req, res){
  res.sendFile(__dirname + '/emc_socket/index.js');
});
/*app.use(function(req, res, next) {
  if (req.originalUrl === "/index.js") {
    console.log("Sending compressed index.js");
    req.url = "/emc_socket/index.js.gz";
    res.setHeader('Content-Encoding', 'gzip');
    res.setHeader('Content-Type', 'application/javascript');
  }
  next();
});*/
wss.on('connection', function connection(ws) {
  console.log('onconnection')
  ws.on('message', function incoming(message) {
    console.log('received: %s', message);
    wss.broadcast(message, ws);
  });
  ws.send(str2ab('something'));
});

wss.broadcast = function broadcast(data, sender) {
  wss.clients.forEach(function each(client) {
    if (client != sender)
      client.send(data);
  });
};
http.listen(3000, function(){
  console.log('listening on *:3000');
});