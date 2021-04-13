import { Users } from '../models/Users.js'

let User = {
  async checkLogin(user, pass){
    if (user === "" || pass === ""){
      throw new Error ("Ingrese usuario/contraseña");
    }
    let result = await Users.getLogin(user, pass);

    if (result.data.status === false){
      throw new Error (userdata.message);
    }

    let userData = result.data;
    let params = { //fixed
      url: "",
      method : "GET",
      contentType: "application/x-www-form-urlencoded",
      crossDomain: true,
      dataType: "json",
      headers: { "Authorization" :  userData.data.token }
    };
    localStorage.setItem("request_params", JSON.stringify(params));
    delete userData.data.token;
    userData["userPermissions"] = [1, 4, 14, 15]; //Owner, Customer, Seller, Deliverer
    localStorage.setItem("userData",JSON.stringify(userData.data));

    return true;
  },
  isLogged(){
    let x = ["userData"];
    let isLogged = true;
    for (var i=0; i<x.length; i++){
      if(localStorage.getItem(x[i]) === undefined || localStorage.getItem(x[i]) === null || localStorage.getItem(x[i]) === "0"){
        isLogged = false;
        break;
      }
    }
    isLogged = this.checkLogOff();
    return isLogged;
  },
  checkLogOff(){
    let req = JSON.parse(localStorage.getItem("request_params"));
    if (req === undefined || req === null) return false;

    let data = this.decodeJWT(req.headers.Authorization);
    var today = new Date(); //Current date
    var expirationDate = new Date(data.expiration);

    if (today.getTime() > expirationDate.getTime()){
      //this.logOff();
      return false;
    } 
    else if (today < expirationDate){
    }
    else{
    }
    return true;
  },
  logOff (){
    localStorage.removeItem("userData");
    localStorage.removeItem("request_params");
    location.reload();
  },
  decodeJWT (token) {
    var base64Url = token.split('.')[1];
    var base64 = decodeURIComponent(atob(base64Url).split('').map(function(c) {
        return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
    }).join(''));

    return JSON.parse(base64);
  },
};

export default User;
