import AjaxHandler from './AjaxHandler.js';
import endpoint from './ConnectionMode.js';

let Users = {
  async getLogin (userdata, password){
    try{
      let params = {
        method : "POST",
        url : `${endpoint}/user/login`,
        data : {
          UserData : userdata,
          UserPassword : password,
        },
        dataType : "json",
      }
      var ajax = new AjaxHandler();
      let result = await ajax.request(params);
      return result;
    }
    catch(err){
      throw err;
    }
  },
};

export { Users };
