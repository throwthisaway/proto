'use strict';
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
var WebRTCPeer = function () {
    var conn, sendChannel, receiveChannel, servers = null, pcConstraint = null
    var ws;
    var recvSize = 0;
    function onAddIceCandidateSuccess() { console.log('onAddIceCandidateSuccess'); }

    function onAddIceCandidateError(error) {
        console.log('Failed to add Ice Candidate: ' + error.toString());
    }

    function iceCb(e) {
        if (!e.candidate) return;
        ws.send(JSON.stringify({ 'candidate': e.candidate }));
        console.log('Send ICE candidate: \n' + e.candidate.candidate);
    }

    function gotDescription(desc) {
        conn.setLocalDescription(desc);
        console.log('gotDescription \n' + desc.sdp);
        ws.send(JSON.stringify({ 'sdp': desc }));
    }

    function onSendChannelStateChange() {
        var readyState = sendChannel.readyState;
        console.log('Send channel state is: ' + readyState);
        if (readyState === 'open') { }
        else /*close*/{ }
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
    function init() {
        if (conn) return;
        conn = new RTCPeerConnection(servers, pcConstraint);
        conn.ondatachannel = onReceiveChannelCb;
        conn.onicecandidate = iceCb;
        var dataChannelParams = { ordered: false };
        sendChannel = conn.createDataChannel('sendDataChannel', dataChannelParams);
        sendChannel.binaryType = 'arraybuffer';
        sendChannel.onopen = onSendChannelStateChange;
        sendChannel.onclose = onSendChannelStateChange;
    }
    var offerSent = false;
    function onWSMessage(e) {
        var incoming = JSON.parse(e.data);
        if (incoming.type === 'connect') {
            offerSent = true;
            conn.createOffer().then(
                gotDescription,
                onCreateSessionDescriptionError
              );
        } else if (incoming.sdp) {
            console.log('onwsmessage-setdesc');
            conn.setRemoteDescription(new RTCSessionDescription(incoming.sdp));
            if (!offerSent) {
                conn.createAnswer().then(
                    gotDescription,
                    onCreateSessionDescriptionError
                );
            }
        }
        else {
            console.log('onwsmessage-addicecandidate');
            conn.addIceCandidate(new RTCIceCandidate(incoming.candidate)).then(
                onAddIceCandidateSuccess,
                onAddIceCandidateError
            );
        }
    }

    return {
        init: function (url) {
            ws = new WebSocket('ws://' + url);
            ws.onopen = function () {
                console.log('ws-onopen');
                ws.send(JSON.stringify({'type' : 'connect'}));
            };

            ws.onerror = function (error) {
                console.log('ws-error ' + error);
            };

            ws.onmessage = function (e) {
                onWSMessage(e);
            };
            init();
        },
        send: function(data) {
            sendChannel.send(data);
            console.log('Sent Data: ' + ab2str(data));
        }
    };
}();
