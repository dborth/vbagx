import Framework7, { getDevice } from './framework7-custom.js';

// Import F7 Styles
import '../css/framework7-custom.less';

// Import Icons and App Custom Styles
import '../css/icons.css';
import '../css/app.less';

// Import Cordova APIs
import cordovaApp from './cordova-app.js';

// Import Routes
import routes from './routes.js';

// Import htpp (axios) instance 
import { http } from './api/config.js';

// Import main app component
import App from '../app.f7.html';

// Import user store
import user from './stores/user.js';

var device = getDevice();
var app = new Framework7({
  name: 'Comida Cab',      // App name
  theme: 'auto',           // Automatic theme detection
  el: '#app',              // App root element
  component: App,          // App main component
  id: 'com.comidacab.app', // App bundle ID
  version: '2.0.0',        // App version id
  routes: routes,          // App routes
  // serviceWorker: {         // Register service worker
  //   path: '/service-worker.js',
  // },

  // view: {
  //   browserHistory: true,
  // },

  dialog: {
    buttonOk: 'Aceptar',
    buttonCancel: 'Cancelar',
  },

  toast: {
    closeTimeout: 2000,
    closeButton: true,
    closeButtonText: "[X]"
  },

  // Input settings
  input: {
    scrollIntoViewOnFocus: device.cordova && !device.electron,
    scrollIntoViewCentered: device.cordova && !device.electron,
  },
  // Cordova Statusbar settings
  statusbar: {
    iosOverlaysWebView: true,
    androidOverlaysWebView: false,
  },
  on: {
    init: function () {
      var f7 = this;
      if (f7.device.cordova) {
        // Init cordova APIs (see cordova-app.js)
        cordovaApp.init(f7);
      }

      //Intercept axios call to determine if it gets (401 Unauthorized) response
      http.interceptors.response.use(undefined, function (err) {
        return new Promise(function (resolve, reject) {
          if (err.status === 401 && err.config && !err.config.__isRetryRequest) {
            console.log("Auto saliendo...");
            user.dispatch('logOut').then( () => { } ); //Untested
          }
          throw err;
        });
      });
    },
  },
});
