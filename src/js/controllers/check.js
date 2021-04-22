//import User from './users.js';
import user from './../stores/user.js';

let Check = {
  authorization( { to, resolve } ) {
    const router = this;
    if (!user.getters.isLogged.value){
    //if (!User.isLogged()){
      router.navigate('/login/', { reloadCurrent: true });
    }
    else{
      resolve(to.path);
    }
  },
  permission( { to, resolve, reject } ) {
    const router = this;
    //const userData = JSON.parse(localStorage.getItem("userData"));
    //const userRoles = userData['userPermissions'];
    const userRoles = user.getters.roles.value;

    let currentIndex = -1;
    for (let i = 0; i < router.routes.length; i++) {
      if (router.routes[i].name == to.name){
        currentIndex = i;
        break;
      }
    }

    let found = false;
    if (currentIndex != -1){
      let totalUserRoles = userRoles.length;
      let allowedRoles = router.routes[currentIndex].allowedRoles
      let totalAllowedRoles = allowedRoles.length;
      for (let i = 0; i < totalUserRoles; i++) {
        for (let j = 0; j < totalAllowedRoles; j++) {
          if (userRoles[i] == allowedRoles[j]){
            found = true;
            break;
          }
        }
        if (found) break;
      }
    }

    if (found){
      resolve(to.path);
      return;
    }
    reject();
    router.navigate('/forbidden/', { reloadCurrent: true });
  },
};

export default Check;
