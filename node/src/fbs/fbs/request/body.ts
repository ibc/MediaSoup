// automatically generated by the FlatBuffers compiler, do not modify

import { CreatePipeTransportRequest, CreatePipeTransportRequestT } from '../../fbs/router/create-pipe-transport-request';
import { CreatePlainTransportRequest, CreatePlainTransportRequestT } from '../../fbs/router/create-plain-transport-request';
import { CreateWebRtcTransportRequest, CreateWebRtcTransportRequestT } from '../../fbs/router/create-web-rtc-transport-request';
import { ConsumeRequest, ConsumeRequestT } from '../../fbs/transport/consume-request';
import { CloseRouterRequest, CloseRouterRequestT } from '../../fbs/worker/close-router-request';
import { CloseWebRtcServerRequest, CloseWebRtcServerRequestT } from '../../fbs/worker/close-web-rtc-server-request';
import { CreateRouterRequest, CreateRouterRequestT } from '../../fbs/worker/create-router-request';
import { CreateWebRtcServerRequest, CreateWebRtcServerRequestT } from '../../fbs/worker/create-web-rtc-server-request';
import { UpdateableSettings, UpdateableSettingsT } from '../../fbs/worker/updateable-settings';


export enum Body {
  NONE = 0,
  FBS_Router_CreateWebRtcTransportRequest = 1,
  FBS_Router_CreatePlainTransportRequest = 2,
  FBS_Router_CreatePipeTransportRequest = 3,
  FBS_Transport_ConsumeRequest = 4,
  FBS_Worker_UpdateableSettings = 5,
  FBS_Worker_CreateWebRtcServerRequest = 6,
  FBS_Worker_CloseWebRtcServerRequest = 7,
  FBS_Worker_CreateRouterRequest = 8,
  FBS_Worker_CloseRouterRequest = 9
}

export function unionToBody(
  type: Body,
  accessor: (obj:CloseRouterRequest|CloseWebRtcServerRequest|ConsumeRequest|CreatePipeTransportRequest|CreatePlainTransportRequest|CreateRouterRequest|CreateWebRtcServerRequest|CreateWebRtcTransportRequest|UpdateableSettings) => CloseRouterRequest|CloseWebRtcServerRequest|ConsumeRequest|CreatePipeTransportRequest|CreatePlainTransportRequest|CreateRouterRequest|CreateWebRtcServerRequest|CreateWebRtcTransportRequest|UpdateableSettings|null
): CloseRouterRequest|CloseWebRtcServerRequest|ConsumeRequest|CreatePipeTransportRequest|CreatePlainTransportRequest|CreateRouterRequest|CreateWebRtcServerRequest|CreateWebRtcTransportRequest|UpdateableSettings|null {
  switch(Body[type]) {
    case 'NONE': return null; 
    case 'FBS_Router_CreateWebRtcTransportRequest': return accessor(new CreateWebRtcTransportRequest())! as CreateWebRtcTransportRequest;
    case 'FBS_Router_CreatePlainTransportRequest': return accessor(new CreatePlainTransportRequest())! as CreatePlainTransportRequest;
    case 'FBS_Router_CreatePipeTransportRequest': return accessor(new CreatePipeTransportRequest())! as CreatePipeTransportRequest;
    case 'FBS_Transport_ConsumeRequest': return accessor(new ConsumeRequest())! as ConsumeRequest;
    case 'FBS_Worker_UpdateableSettings': return accessor(new UpdateableSettings())! as UpdateableSettings;
    case 'FBS_Worker_CreateWebRtcServerRequest': return accessor(new CreateWebRtcServerRequest())! as CreateWebRtcServerRequest;
    case 'FBS_Worker_CloseWebRtcServerRequest': return accessor(new CloseWebRtcServerRequest())! as CloseWebRtcServerRequest;
    case 'FBS_Worker_CreateRouterRequest': return accessor(new CreateRouterRequest())! as CreateRouterRequest;
    case 'FBS_Worker_CloseRouterRequest': return accessor(new CloseRouterRequest())! as CloseRouterRequest;
    default: return null;
  }
}

export function unionListToBody(
  type: Body, 
  accessor: (index: number, obj:CloseRouterRequest|CloseWebRtcServerRequest|ConsumeRequest|CreatePipeTransportRequest|CreatePlainTransportRequest|CreateRouterRequest|CreateWebRtcServerRequest|CreateWebRtcTransportRequest|UpdateableSettings) => CloseRouterRequest|CloseWebRtcServerRequest|ConsumeRequest|CreatePipeTransportRequest|CreatePlainTransportRequest|CreateRouterRequest|CreateWebRtcServerRequest|CreateWebRtcTransportRequest|UpdateableSettings|null, 
  index: number
): CloseRouterRequest|CloseWebRtcServerRequest|ConsumeRequest|CreatePipeTransportRequest|CreatePlainTransportRequest|CreateRouterRequest|CreateWebRtcServerRequest|CreateWebRtcTransportRequest|UpdateableSettings|null {
  switch(Body[type]) {
    case 'NONE': return null; 
    case 'FBS_Router_CreateWebRtcTransportRequest': return accessor(index, new CreateWebRtcTransportRequest())! as CreateWebRtcTransportRequest;
    case 'FBS_Router_CreatePlainTransportRequest': return accessor(index, new CreatePlainTransportRequest())! as CreatePlainTransportRequest;
    case 'FBS_Router_CreatePipeTransportRequest': return accessor(index, new CreatePipeTransportRequest())! as CreatePipeTransportRequest;
    case 'FBS_Transport_ConsumeRequest': return accessor(index, new ConsumeRequest())! as ConsumeRequest;
    case 'FBS_Worker_UpdateableSettings': return accessor(index, new UpdateableSettings())! as UpdateableSettings;
    case 'FBS_Worker_CreateWebRtcServerRequest': return accessor(index, new CreateWebRtcServerRequest())! as CreateWebRtcServerRequest;
    case 'FBS_Worker_CloseWebRtcServerRequest': return accessor(index, new CloseWebRtcServerRequest())! as CloseWebRtcServerRequest;
    case 'FBS_Worker_CreateRouterRequest': return accessor(index, new CreateRouterRequest())! as CreateRouterRequest;
    case 'FBS_Worker_CloseRouterRequest': return accessor(index, new CloseRouterRequest())! as CloseRouterRequest;
    default: return null;
  }
}
