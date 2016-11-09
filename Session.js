"use strict";
const WS = require('ws');
exports.WS = WS;
let clientIDLen = 5;
function generate0ToOClientID(count) {
    //var symbols = '0123456789:;<=>?@ABCDEFGHIJKLMNO',
    var res = '';
    for (var i = 0; i < count; ++i) {
        res += String.fromCharCode((48 + Math.random() * 32) | 0);
    }
    return "" + res;
}
class Client {
    constructor(ws) {
        this.ctrl = 0;
        this.id = generate0ToOClientID(clientIDLen);
        this.ws = ws;
    }
    close() { this.ws.close(); }
    sendSessionStringMessage(str) {
        this.ws.send(JSON.stringify({ 'session': str }));
    }
}
exports.Client = Client;
class Session {
    constructor() {
        this.clients = [];
    }
    broadcastStringToSession(sender, data) {
        this.clients.forEach(function each(client) {
            try {
                if (client != sender)
                    client.sendSessionStringMessage(data);
            }
            catch (err) {
                console.log('client unexpectedly closed: ' + err.message);
            }
        });
    }
    broadcastToSession(sender, data) {
        this.clients.forEach(function each(client) {
            try {
                if (client != sender)
                    client.ws.send(data);
            }
            catch (err) {
                console.log('client unexpectedly closed: ' + err.message);
            }
        });
    }
    findClientToCtrl() {
        for (var i = 0; i < this.clients.length; ++i) {
            if (this.clients[i].ctrl === 0)
                return this.clients[i];
        }
        return null;
    }
    findClientByID(id) {
        for (var i = 0; i < this.clients.length; ++i) {
            if (this.clients[i].id === id)
                return this.clients[i];
        }
        return null;
    }
    removeClient(client) {
        this.clients.splice(this.clients.indexOf(client), 1);
    }
}
exports.Session = Session;
