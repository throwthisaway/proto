'use strict';
// TODO:: error handling
// TODO:: close handling
//chrome://webrtc-internals/
var debug = true;
function debugOut(msg) {
    if (debug)
        console.log(msg);
}
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
var createConnection = function () {
    var ws, remoteID, clientID;
    var recvSize = 0;
    var conn, sendChannel, receiveChannel, servers = null, pcConstraint = null;
    var onMessageCb = null, onCloseCb = null;

    function onAddIceCandidateSuccess() { debugOut('onAddIceCandidateSuccess'); }

    function onAddIceCandidateError(error) {
        debugOut('Failed to add Ice Candidate: ' + error.toString());
    }

    function iceCb(e) {
        if (!e.candidate) return;
        ws.send(JSON.stringify({ 'targetID': remoteID, 'originID': clientID, 'candidate': e.candidate }));
        debugOut('Send ICE candidate: \n' + e.candidate.candidate);
    }

    function gotOfferDescription(desc) {
        conn.setLocalDescription(desc);
        debugOut('gotOfferDescription \n' + desc.sdp);
        ws.send(JSON.stringify({ 'offer': { 'targetID': remoteID, 'originID': clientID, 'sdp': desc } }));
    }

    function gotAnswerDescription(desc) {
        conn.setLocalDescription(desc);
        debugOut('gotAnswerDescription \n' + desc.sdp);
        ws.send(JSON.stringify({ 'answer': { 'targetID': remoteID, 'originID': clientID, 'sdp': desc } }));
    }

    function onSendChannelStateChange() {
        var readyState = sendChannel.readyState;
        debugOut('Send channel state is: ' + readyState);
        if (readyState === 'open') { }
        else if (readyState === 'close') { onCloseCb(); }
    }

    function onAddIceCandidateSuccess() {
        debugOut('AddIceCandidate success.');
    }

    function onAddIceCandidateError(error) {
        debugOut('Failed to add Ice Candidate: ' + error.toString());
    }

    function onCreateSessionDescriptionError(error) {
        debugOut('Failed to create session description: ' + error.toString());
    }

    function onReceiveMsgCb(e) {
        recvSize += e.data.length;
        debugOut('Received: ' + ab2str(e.data));
        if (onMessageCb) onMessageCb(remoteID, e.data);
    }

    function onReceiveChannelStateChange() {
        var readyState = receiveChannel.readyState;
        debugOut('Receive channel state is: ' + readyState);
        if (readyState === 'close') { onCloseCb(); }
    }

    function onReceiveChannelCb(e) {
        debugOut('Receive Channel Callback');
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
        if (onCloseCb) onCloseCb(remoteID);
    }

    function onCloseCb() {
        debugOut('onclose');
        close();
    }

    function init(_ws, _remoteID, _clientID, _onMessageCb, _onCloseCb) {
        ws = _ws;
        remoteID = _remoteID;
        clientID = _clientID;
        onMessageCb = _onMessageCb;
        onCloseCb = _onCloseCb;
        conn = new RTCPeerConnection(servers, pcConstraint);
        conn.ondatachannel = onReceiveChannelCb;
        conn.onicecandidate = iceCb;
        conn.onclose = onCloseCb;
        var dataChannelParams = { ordered: false };
        sendChannel = conn.createDataChannel('sendDataChannel', dataChannelParams);
        sendChannel.binaryType = 'arraybuffer';
        sendChannel.onopen = onSendChannelStateChange;
        sendChannel.onclose = onSendChannelStateChange;
    }

    function send(data) {
        sendChannel.send(data);
        debugOut('Sent Data: ' + ab2str(data));
    }

    function handleOffer(sdp) {
        debugOut('handleOffer');
        conn.setRemoteDescription(new RTCSessionDescription(sdp));
        conn.createAnswer().then(
            gotAnswerDescription,
            onCreateSessionDescriptionError
        );
    }

    function handleAnswer(sdp) {
        debugOut('handleAnswer');
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
        close: close
    };
}

var WebRTCPeer = function () {
    var ws;
    var clientID;
    var connections = new Map();
    var onMessageCb = null, onCloseCb = null;
    function onMessage(remoteID, data) {
        if (onMessageCb) onMessageCb(remoteID, data);
    }
    function onClose(remoteID) {
        connections.delete(remoteID);
        if (onCloseCb) onCloseCb(remoteID);
    }
    function onWSMessage(e) {
        var msg = JSON.parse(e.data);
        if (msg.connect) {
            var conn = createConnection();
            connections.set(msg.connect, conn);
            conn.init(ws, msg.connect, clientID, onMessage, onClose);
            conn.sendOffer();
        } else if (msg.offer) {
            debugOut('onwsmessage-on-offer');
            var conn = createConnection();
            connections.set(msg.offer.originID, conn);
            conn.init(ws, msg.offer.originID, clientID/*same as  msg.offer.targetID*/, onMessage, onClose);
            conn.handleOffer(msg.offer.sdp);
        } else if (msg.answer) {
            debugOut('onwsmessage-on-answer');
            var conn = connections.get(msg.answer.originID);
            if (conn) conn.handleAnswer(msg.answer.sdp);
        } else if (msg.candidate) {
            debugOut('onwsmessage-addicecandidate');
            var conn = connections.get(msg.originID);
            if (conn) conn.handleIceCandidate(msg.candidate);
        } else if (msg.close) {
            var conn = connections.get(msg.close);
            if (conn) conn.close();
            connections.delete(msg.close);
        }
    }

    function setOnMessage(cb) { onMessageCb = cb; }
    function setOnClose(cb) { onCloseCb = cb; }

    return {
        init: function (url, _clientID) {
            clientID = _clientID;
            ws = new WebSocket('ws://' + url);
            ws.onopen = function () {
                debugOut('ws-onopen');
                ws.send(JSON.stringify({'connect' : clientID}));
            };

            ws.onerror = function (error) {
                debugOut('ws-error ' + error);
            };

            ws.onmessage = function (e) {
                onWSMessage(e);
            };
        },
        send: function (data) {
            for (var client of connections.values()) {
                client.send(data);
            }
        },
        setOnMessage: setOnMessage,
        setOnClose: setOnClose
        // TODO:: onError
    };
}();
