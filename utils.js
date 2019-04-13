"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
class Debug {
    constructor(debug, debugRTC) {
        this.debug = true;
        this.debugRTC = true;
        this.debug = debug;
        this.debugRTC = debugRTC;
    }
    Log(msg) {
        this.debug && console.log(msg);
    }
    LogRTC(msg) {
        this.debugRTC && console.log(msg);
    }
}
exports.Debug = Debug;
function ab2strUtf16(buf) {
    return String.fromCharCode.apply(null, new Uint16Array(buf));
}
exports.ab2strUtf16 = ab2strUtf16;
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
exports.ab2str = ab2str;
function str2abUtf16(str) {
    var buf = new ArrayBuffer(str.length * 2); // 2 bytes for each char
    var bufView = new Uint16Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; ++i) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
exports.str2abUtf16 = str2abUtf16;
function str2ab(str) {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; ++i) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
exports.str2ab = str2ab;
