'use strict';
// TODO:: error handling
// TODO:: close handling
//chrome://webrtc-internals/
var debug = new Debug(false, false);
var createConnection = function (initial) {
    var ws, remoteID, clientID;
    var recvSize = 0;
    var conn, sendChannel, receiveChannel, servers = null, pcConstraint = null;

    function Connected() {
        if (sendChannel && sendChannel.readyState === 'open' &&
           receiveChannel && receiveChannel.readyState === 'open')
            WebRTCPeer.connected(remoteID, initial);
    }

    function onAddIceCandidateSuccess() { debugOutRTC('onAddIceCandidateSuccess'); }

    function onAddIceCandidateError(error) {
        debug.LogRTC('Failed to add Ice Candidate: ' + error.toString());
    }

    function iceCb(e) {
        debug.LogRTC('Send ICE candidate: \n' + e.candidate);
        if (!e.candidate) return;
        ws.send(JSON.stringify({ 'targetID': remoteID, 'originID': clientID, 'candidate': e.candidate }));
    }

    function gotOfferDescription(desc) {
        debug.LogRTC('gotOfferDescription \n' + desc.sdp);
        conn.setLocalDescription(desc);
        ws.send(JSON.stringify({ 'offer': { 'targetID': remoteID, 'originID': clientID, 'sdp': desc } }));
    }

    function gotAnswerDescription(desc) {
        debug.LogRTC('gotAnswerDescription \n' + desc.sdp);
        conn.setLocalDescription(desc);
        ws.send(JSON.stringify({ 'answer': { 'targetID': remoteID, 'originID': clientID, 'sdp': desc } }));
    }

    function onSendChannelStateChange() {
        var readyState = sendChannel.readyState;
        debug.LogRTC('Send channel state is: ' + readyState);
        if (readyState === 'open') { Connected(); }
        else if (readyState === 'close') { close(); }
    }

    function onAddIceCandidateSuccess() {
        debug.LogRTC('AddIceCandidate success.');
    }

    function onAddIceCandidateError(error) {
        debug.LogRTC('Failed to add Ice Candidate: ' + error.toString());
    }

    function onCreateSessionDescriptionError(error) {
        debug.LogRTC('Failed to create session description: ' + error.toString());
    }

    function onReceiveMsgCb(e) {
        recvSize += e.data.length;
        WebRTCPeer.onmessage(remoteID, e.data);
    }

    function onReceiveChannelStateChange() {
        var readyState = receiveChannel.readyState;
        debug.LogRTC('Receive channel state is: ' + readyState);
        if (readyState === 'open') { Connected(); }
        else if (readyState === 'close') { close(); }
    }

    function onReceiveChannelCb(e) {
        debug.LogRTC('Receive Channel Callback');
        receiveChannel = e.channel;
        receiveChannel.binaryType = 'arraybuffer';
        receiveChannel.onmessage = onReceiveMsgCb;
        receiveChannel.onopen = onReceiveChannelStateChange;
        receiveChannel.onclose = onReceiveChannelStateChange;
        recvSize = 0;
    }

    function close() {
        if (sendChannel)
            sendChannel.close();
        if (receiveChannel)
            receiveChannel.close();
        conn.close();
        conn = null;
        WebRTCPeer.close(remoteID);
    }

    function init(_ws, _remoteID, _clientID) {
        ws = _ws;
        remoteID = _remoteID;
        clientID = _clientID;
        conn = new RTCPeerConnection(servers, pcConstraint);
        conn.ondatachannel = onReceiveChannelCb;
        conn.onicecandidate = iceCb;
        conn.onclose = close;
        var dataChannelParams = { ordered: false };
        sendChannel = conn.createDataChannel('sendDataChannel', dataChannelParams);
        sendChannel.binaryType = 'arraybuffer';
        sendChannel.onopen = onSendChannelStateChange;
        sendChannel.onclose = onSendChannelStateChange;
    }

    function send(data) {
        if (sendChannel.readyState == 'open')
            sendChannel.send(data);
    }

    function handleOffer(sdp) {
        debug.LogRTC('handleOffer');
        conn.setRemoteDescription(new RTCSessionDescription(sdp));
        conn.createAnswer().then(
            gotAnswerDescription,
            onCreateSessionDescriptionError
        );
    }

    function handleAnswer(sdp) {
        debug.LogRTC('handleAnswer');
        conn.setRemoteDescription(new RTCSessionDescription(sdp));
    }

    function sendOffer() {
        conn.createOffer().then(
            gotOfferDescription,
            onCreateSessionDescriptionError);
    }

    function handleIceCandidate(candidate) {
        conn.addIceCandidate(new RTCIceCandidate(candidate)).then(
            onAddIceCandidateSuccess,
            onAddIceCandidateError);
    }

    return {
        init: init,
        send: send,
        handleOffer: handleOffer,
        handleAnswer: handleAnswer,
        sendOffer: sendOffer,
        handleIceCandidate: handleIceCandidate,
        close: close,
        initial: initial
    };
}

var WebRTCPeer = function () {
    var ws;
    var clientID;
    var cpp = null;
    var connections = new Map();

    function onWSMessage(e) {
        var msg = JSON.parse(e.data);
        if (msg.session) {
            if (cpp) cpp.WSOnMessage(msg.session);
        } else if (msg.connect) {
            var conn = createConnection(true);
            connections.set(msg.connect, conn);
            conn.init(ws, msg.connect, clientID);
            conn.sendOffer();
        } else if (msg.offer) {
            debug.LogRTC('onwsmessage-on-offer');
            var conn = createConnection(false);
            connections.set(msg.offer.originID, conn);
            conn.init(ws, msg.offer.originID, clientID/*same as msg.offer.targetID*/);
            conn.handleOffer(msg.offer.sdp);
        } else if (msg.answer) {
            debug.LogRTC('onwsmessage-on-answer');
            var conn = connections.get(msg.answer.originID);
            if (conn) conn.handleAnswer(msg.answer.sdp);
        } else if (msg.candidate) {
            debug.LogRTC('onwsmessage-addicecandidate');
            var conn = connections.get(msg.originID);
            if (conn) conn.handleIceCandidate(msg.candidate);
        } else if (msg.close) {
            var conn = connections.get(msg.close);
            if (conn) conn.close();
            connections.delete(msg.close);
        }
    }

    return {
        init: function (url, _clientID, _cpp) {
            clientID = _clientID;
            cpp = _cpp;
            ws = new WebSocket('ws://' + url);
            ws.onopen = function () {
                //debugOut('ws-onopen');
                cpp && cpp.OnOpen();
            };

            ws.onerror = function (error) {
                debug.Log('ws-error ' + error);
            };
            ws.onclose = function(code, message){
                cpp && cpp.WSOnClose();
            },
            ws.onmessage = function (e) {
                onWSMessage(e);
            };
        },
        close: function (remoteID) {
            connections.delete(remoteID);
            cpp && cpp.OnClose();
        },
        send: function (data) {
            for (var client of connections.values()) {
                client.send(data);
            }
        },
        wsSend: function (data) {
            ws.send(JSON.stringify({ 'session': ab2str(data) }));
        },
        connected: function (remoteID, initial) {
            if (cpp) cpp.OnConnect(initial);
            //var conn = connections.get(remoteID);
            //var clientID = generate0ToOClientID(clientIDLen);
            //conn.send(str2ab("CONN" + clientID + "0"));
            /*TODO::*/
        },
        onmessage: function (remoteID, data) { if (cpp) cpp.OnMessage(data); },
        rtcConnect: function () { ws.send(JSON.stringify({ 'connect': clientID })); }
        // TODO:: onError
    };
}();
