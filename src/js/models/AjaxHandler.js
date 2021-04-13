import Framework7 from '../framework7-custom.js';

class AjaxHandler{
  constructor() {
    this.url = "";
    this.data = {};
    this.method = "GET";
    this.contentType = "application/x-www-form-urlencoded";
    this.crossDomain = true;
    this.dataType = "json";
    this.headers = { "Authorization" :  "" };
  }
  async request(params){
    let self = this;
    let getParams = (params !== undefined);
    return Framework7.request({
      headers: !(getParams && params.headers) ? self.headers : params.headers,
      url: !(getParams && params.url) ? self.url : params.url, /* request URL */
      dataType: !(getParams && params.dataType) ? self.dataType : params.dataType,
      contentType: !(getParams && params.contentType) ? self.contentType : params.contentType,
      crossDomain: true,
      method: !(getParams && params.method) ? self.method : params.method,
      data: !(getParams && params.data) ? self.data : params.data
    });
  }
};

export default AjaxHandler;
