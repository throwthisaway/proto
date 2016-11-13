"use strict";
var Debug = (function () {
    function Debug(debug, debugRTC) {
        this.debug = true;
        this.debugRTC = true;
        this.debug = debug;
        this.debugRTC = debugRTC;
    }
    Debug.prototype.Log = function (msg) {
        this.debug && console.log(msg);
    };
    Debug.prototype.LogRTC = function (msg) {
        this.debugRTC && console.log(msg);
    };
    return Debug;
}());
function ab2strUtf16(buf) {
    return String.fromCharCode.apply(null, new Uint16Array(buf));
}
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
function str2abUtf16(str) {
    var buf = new ArrayBuffer(str.length * 2); // 2 bytes for each char
    var bufView = new Uint16Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; ++i) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
function str2ab(str) {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; ++i) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
function generate0ToOClientID(count) {
    //var symbols = '0123456789:;<=>?@ABCDEFGHIJKLMNO',
    var res = '';
    for (var i = 0; i < count; ++i) {
        res += String.fromCharCode((48 + Math.random() * 32) | 0);
    }
    return "" + res;
}
