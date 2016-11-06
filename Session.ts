import WS = require('ws');
class Client {
    public session: Session;
    public ctrl: number;
    public clientID: string;
    public ws: WS;
    constructor(ws:WS) { this.ws = ws; }
    public close() { this.ws.close();}
    public sendSessionStringMessage(str : string) {
        this.ws.send(JSON.stringify({'session': str})); }
}
class Session {
    public id: string;
    public clients: Client[];
    constructor() { this.clients = []; }

    public broadcastStringToSession(sender : Client | null, data : string) {
        this.clients.forEach(function each(client) {
            try{
                if (client != sender) client.sendSessionStringMessage(data);
            } catch(err) {
                console.log('client unexpectedly closed: ' + err.message); }
        });
    }
    public broadcastToSession(sender : Client, data : any) {
        this.clients.forEach(function each(client) {
            try{
                if (client != sender)
                    client.ws.send(data);
            } catch(err) {
                console.log('client unexpectedly closed: ' + err.message); }
        });
    }
    public findClientToCtrl() : Client | null {
        for (var i = 0; i < this.clients.length; ++i) {
            if (this.clients[i].ctrl === 0) return this.clients[i];
        }
        return null;
    }
    public findClientByID(id : string) : Client | null {
        for (var i = 0; i < this.clients.length; ++i) {
            if (this.clients[i].clientID === id) return this.clients[i];
        }
        return null;
    }
    public removeClient(client: Client){
        this.clients.splice(this.clients.indexOf(client));
    }
}

export {Client, Session, WS}