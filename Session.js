"use strict";
var WS = require('ws');
exports.WS = WS;
var Client = (function () {
    function Client(ws) {
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
            if (this.clients[i].clientID === id)
                return this.clients[i];
        }
        return null;
    };
    Session.prototype.removeClient = function (client) {
        this.clients.splice(this.clients.indexOf(client));
    };
    return Session;
}());
exports.Session = Session;
