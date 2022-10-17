"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ConsumeRequestT = exports.ConsumeRequest = exports.RequestT = exports.Request = exports.Method = exports.unionListToBody = exports.unionToBody = exports.Body = void 0;
var body_1 = require("./f-b-s/request/body");
Object.defineProperty(exports, "Body", { enumerable: true, get: function () { return body_1.Body; } });
Object.defineProperty(exports, "unionToBody", { enumerable: true, get: function () { return body_1.unionToBody; } });
Object.defineProperty(exports, "unionListToBody", { enumerable: true, get: function () { return body_1.unionListToBody; } });
var method_1 = require("./f-b-s/request/method");
Object.defineProperty(exports, "Method", { enumerable: true, get: function () { return method_1.Method; } });
var request_1 = require("./f-b-s/request/request");
Object.defineProperty(exports, "Request", { enumerable: true, get: function () { return request_1.Request; } });
Object.defineProperty(exports, "RequestT", { enumerable: true, get: function () { return request_1.RequestT; } });
var consume_request_1 = require("./f-b-s/transport/consume-request");
Object.defineProperty(exports, "ConsumeRequest", { enumerable: true, get: function () { return consume_request_1.ConsumeRequest; } });
Object.defineProperty(exports, "ConsumeRequestT", { enumerable: true, get: function () { return consume_request_1.ConsumeRequestT; } });
