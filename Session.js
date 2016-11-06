"use strict";
var WS = require('ws');
exports.WS = WS;
var clientIDLen = 5;
function generate0ToOClientID(count) {
    //var symbols = '0123456789:;<=>?@ABCDEFGHIJKLMNO',
    var res = '';
    for (var i = 0; i < count; ++i) {
        res += String.fromCharCode((48 + Math.random() * 32) | 0);
    }
    return "" + res;
}
var Client = (function () {
    function Client(ws) {
        this.ctrl = 0;
        this.id = generate0ToOClientID(clientIDLen);
        this.ws = ws;
    }
    Client.prototype.close = function () { this.ws.close(); };
    Client.prototype.sendSessionStringMessage = function (str) {
        this.ws.send(JSON.stringify({ 'session': str }));
    };
    return Client;
}());
exports.Client = Client;
var Session = (function () {
    function Session() {
        this.clients = [];
    }
    Session.prototype.broadcastStringToSession = function (sender, data) {
        this.clients.forEach(function each(client) {
            try {
                if (client != sender)
                    client.sendSessionStringMessage(data);
            }
            catch (err) {
                console.log('client unexpectedly closed: ' + err.message);
            }
        });
    };
    Session.prototype.broadcastToSession = function (sender, data) {
        this.clients.forEach(function each(client) {
            try {
                if (client != sender)
                    client.ws.send(data);
            }
            catch (err) {
                console.log('client unexpectedly closed: ' + err.message);
            }
        });
    };
    Session.prototype.findClientToCtrl = function () {
        for (var i = 0; i < this.clients.length; ++i) {
            if (this.clients[i].ctrl === 0)
                return this.clients[i];
        }
        return null;
    };
    Session.prototype.findClientByID = function (id) {
        for (var i = 0; i < this.clients.length; ++i) {
            if (this.clients[i].id === id)
                return this.clients[i];
        }
        return null;
    };
    Session.prototype.removeClient = function (client) {
        this.clients.splice(this.clients.indexOf(client), 1);
    };
    return Session;
}());
exports.Session = Session;
