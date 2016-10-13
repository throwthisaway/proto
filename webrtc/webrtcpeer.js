'use strict';
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
var WebRTCPeer = function () {
    var sendConn, recvConn, sendChannel, receiveChannel, servers = null, pcConstraint = null
    var ws;
    var recvSize = 0;
    function onAddIceCandidateSuccess() { console.log('onAddIceCandidateSuccess'); }

    function onAddIceCandidateError(error) {
        console.log('Failed to add Ice Candidate: ' + error.toString());
    }

    function iceSendCb(e) {
        if (!e.candidate) return;
        ws.send(JSON.stringify({ 'type': 'recv', 'candidate': e.candidate }));
        console.log('Send ICE candidate: \n' + e.candidate.candidate);
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

    function gotSendDescription(desc) {
        sendConn.setLocalDescription(desc);
        console.log('Offer from localConnection \n' + desc.sdp);
        ws.send(JSON.stringify({ 'type': 'recv', 'sdp': desc }));
    }

    function iceRecvCb(e) {
        if (!e.candidate) return;
        ws.send(JSON.stringify({ 'type': 'send', 'candidate': e.candidate }));
        console.log('Recv ICE candidate: \n' + e.candidate.candidate);
    }

    function gotRecvDescription(desc) {
        recvConn.setLocalDescription(desc);
        console.log('Answer from remoteConnection \n' + desc.sdp);
        ws.send(JSON.stringify({ 'type': 'send', 'sdp': desc}));
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

    function onWSMessage(e) {
        var incoming = JSON.parse(e.data);
        if (incoming.type === 'recv') {
            if (incoming.sdp) {
                console.log('onwsmessage-remote-setdesc');
                recvConn.setRemoteDescription(new RTCSessionDescription(incoming.sdp));
                recvConn.createAnswer().then(
                  gotRecvDescription,
                  onCreateSessionDescriptionError
                );
            } else {
                console.log('onwsmessage-remote-addicecandidate');
                recvConn.addIceCandidate(new RTCIceCandidate(incoming.candidate)).then(
                  onAddIceCandidateSuccess,
                  onAddIceCandidateError
                );
            }
        } else if (incoming.type === 'send') {
            if (incoming.sdp) {
                console.log('onwsmessage-send-setdesc');
                sendConn.setRemoteDescription(new RTCSessionDescription(incoming.sdp));
            }
            else {
                console.log('onwsmessage-send-addicecandidate');
                sendConn.addIceCandidate(new RTCIceCandidate(incoming.candidate)).then(
                  onAddIceCandidateSuccess,
                  onAddIceCandidateError
                );
            }
        }
    }

    return {
        init: function (url) {
            ws = new WebSocket('ws://' + url);
            ws.onopen = function () {
                console.log('ws-onopen');
            };

            ws.onerror = function (error) {
                console.log('ws-error ' + error);
            };

            ws.onmessage = function (e) {
                onWSMessage(e);
            };

            sendConn = new RTCPeerConnection(servers, pcConstraint);
            var dataChannelParams = { ordered: false };
            sendChannel = sendConn.createDataChannel('sendDataChannel', dataChannelParams);
            sendChannel.binaryType = 'arraybuffer';
            sendChannel.onopen = onSendChannelStateChange;
            sendChannel.onclose = onSendChannelStateChange;
            sendConn.onicecandidate = iceSendCb;

            recvConn = new RTCPeerConnection(servers, pcConstraint);
            recvConn.onicecandidate = iceRecvCb;
            recvConn.ondatachannel = onReceiveChannelCb;

            sendConn.createOffer().then(
                gotSendDescription,
                onCreateSessionDescriptionError
              );
        },
        send: function(data) {
            sendChannel.send(data);
            console.log('Sent Data: ' + ab2str(data));
        }
    };
}();
