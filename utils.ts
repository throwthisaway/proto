export class Debug {
    private debug : boolean = true;
    private debugRTC : boolean = true;
    constructor(debug:boolean, debugRTC:boolean){
        this.debug = debug; this.debugRTC = debugRTC;
    }
    public Log(msg : any) {
        this.debug && console.log(msg);
    }
    public LogRTC(msg : string) {
        this.debugRTC && this.Log(msg);
    }
}

export function ab2strUtf16(buf : number[]) : string {
    return String.fromCharCode.apply(null, new Uint16Array(buf));
}
export function ab2str(buf : number[]) : string {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
}
export function str2abUtf16(str: string)  : ArrayBuffer {
    var buf = new ArrayBuffer(str.length * 2); // 2 bytes for each char
    var bufView = new Uint16Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; ++i) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
export function str2ab(str : string)  : ArrayBuffer {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);
    for (var i = 0, strLen = str.length; i < strLen; ++i) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}
