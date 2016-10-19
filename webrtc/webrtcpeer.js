'use strict';
//chrome://webrtc-internals/
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
var createConnection = function () {
    var ws, remoteID, clientID;
    var recvSize = 0;
    var conn, sendChannel, receiveChannel, servers = null, pcConstraint = null;

    function onAddIceCandidateSuccess() { console.log('onAddIceCandidateSuccess'); }

    function onAddIceCandidateError(error) {
        console.log('Failed to add Ice Candidate: ' + error.toString());
    }

    function iceCb(e) {
        if (!e.candidate) return;
        // TODO::
        ws.send(JSON.stringify({ 'targetID': remoteID, 'originID': clientID, 'candidate': e.candidate }));
        console.log('Send ICE candidate: \n' + e.candidate.candidate);
    }

    function gotOfferDescription(desc) {
        conn.setLocalDescription(desc);
        console.log('gotOfferDescription \n' + desc.sdp);
        ws.send(JSON.stringify({ 'offer': { 'targetID': remoteID, 'originID': clientID, 'sdp': desc } }));
    }

    function gotAnswerDescription(desc) {
        conn.setLocalDescription(desc);
        console.log('gotAnswerDescription \n' + desc.sdp);
        ws.send(JSON.stringify({ 'answer': { 'targetID': remoteID, 'originID': clientID, 'sdp': desc } }));
    }

    function onSendChannelStateChange() {
        var readyState = sendChannel.readyState;
        console.log('Send channel state is: ' + readyState);
        if (readyState === 'open') { }
        else if (readyState === 'close') { onCloseCb(); }
    }

    function onAddIceCandidateSuccess() {
        console.log('AddIceCandidate success.');
    }

    function onAddIceCandidateError(error) {
        console.log('Failed to add Ice Candidate: ' + error.toString());
    }

    function onCreateSessionDescriptionError(error) {
        console.log('Failed to create session description: ' + error.toString());
    }

    function onReceiveMsgCb(e) {
        recvSize += e.data.length;
        console.log('Received: ' + ab2str(e.data));
    }

    function onReceiveChannelStateChange() {
        var readyState = receiveChannel.readyState;
        console.log('Receive channel state is: ' + readyState);
        if (readyState === 'close') { onCloseCb(); }
    }

    function onReceiveChannelCb(e) {
        console.log('Receive Channel Callback');
        receiveChannel = e.channel;
        receiveChannel.binaryType = 'arraybuffer';
        receiveChannel.onmessage = onReceiveMsgCb;
        receiveChannel.onopen = onReceiveChannelStateChange;
        receiveChannel.onclose = onReceiveChannelStateChange;
        recvSize = 0;
    }

    function onCloseCb() {
        console.log('onclose');
        if (sendChannel)
            sendChannel.close();
        if (receiveChannel)
            receiveChannel.close();
        conn.close();
        conn = null;
        offerSent = false;
    }

    function init(_ws, _remoteID, _clientID) {
        ws = _ws;
        remoteID = _remoteID;
        clientID = _clientID;
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
        console.log('Sent Data: ' + ab2str(data));
    }

    function handleOffer(sdp) {
        console.log('handleOffer');
        conn.setRemoteDescription(new RTCSessionDescription(sdp));
        conn.createAnswer().then(
            gotAnswerDescription,
            onCreateSessionDescriptionError
        );
    }

    function handleAnswer(sdp) {
        console.log('handleAnswer');
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
        handleIceCandidate: handleIceCandidate
        /* TODO:: onmessage(), onclose, etc*/
    };
}

var WebRTCPeer = function () {
    var ws;
    var offerSent = false;

    var clientID = Math.random();
    var connections = new Map();

    function onWSMessage(e) {
        var msg = JSON.parse(e.data);
        if (msg.connect) {
            var conn = createConnection();
            connections.set(msg.connect, conn);
            conn.init(ws, msg.connect, clientID);
            conn.sendOffer();
        } else if (msg.offer) {
            console.log('onwsmessage-on-offer');
            var conn = createConnection();
            connections.set(msg.offer.originID, conn);
            conn.init(ws, msg.offer.originID, clientID/*same as  msg.offer.targetID*/);
            conn.handleOffer(msg.offer.sdp);
        } else if (msg.answer) {
            console.log('onwsmessage-on-answer');
            var conn = connections.get(msg.answer.originID);
            if (conn) conn.handleAnswer(msg.answer.sdp);
        } else if (msg.candidate) {
            console.log('onwsmessage-addicecandidate');
            var conn = connections.get(msg.originID);
            if (conn) conn.handleIceCandidate(msg.candidate);
        }
    }

    return {
        init: function (url) {
            ws = new WebSocket('ws://' + url);
            ws.onopen = function () {
                console.log('ws-onopen');
                ws.send(JSON.stringify({'connect' : clientID}));
            };

            ws.onerror = function (error) {
                console.log('ws-error ' + error);
            };

            ws.onmessage = function (e) {
                onWSMessage(e);
            };
        },
        send: function (data) {
            for (var client of connections.values()) {
                client.send(data);
            }
        }
    };
}();
