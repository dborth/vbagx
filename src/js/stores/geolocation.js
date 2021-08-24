import { createStore } from 'framework7';
import { container } from 'webpack';
import { http, localForage } from './../api/config.js';

const geolocation = createStore({
  state:{
    position: {
      // latitude: 0.00,
      // longitude: 0.00,
    },
    container: 'map',
  },
  actions: {
//Mutations
    setPosition({ state }, result) {
      state.position = result;
    },
    setContainer({ state }, result){
      state.container = result;
    },
//End mutations
    getMapLocation({ dispatch}, item){
      dispatch('setContainer', item);
      return new Promise((resolve, reject) => {
        navigator.geolocation.getCurrentPosition(
          position => { 
            resolve(this.mapSuccess(position));
          },
          error => { reject (this.mapError(error));
          },
          { enableHighAccuracy: true }
        );
      });
      //navigator.geolocation.getCurrentPosition(this.mapSuccess, this.mapError, { enableHighAccuracy: true });
    },

    getMap (latitude, longitude){
    },

    getLocationData({ dispatch }){
      return new Promise((resolve, reject) => {
        navigator.geolocation.getCurrentPosition(
          position => {
            dispatch('setPosition', position);
            resolve(position);
          },
          error => { reject(this.mapError(error)); }, 
          { enableHighAccuracy: true }
        );
      });
    },

    mapSuccess(position){
      let mapOptions = {
        center: new google.maps.LatLng(0, 0),
        zoom: 1,
        mapTypeId: google.maps.MapTypeId.ROADMAP
      };
      const map = new google.maps.Map(document.getElementById(this.state.container), mapOptions);
      const latLong = new google.maps.LatLng(position.coords.latitude, position.coords.longitude);

      const marker = new google.maps.Marker({
        position: latLong
      });

      marker.setMap(map);
      map.setZoom(15);
      map.setCenter(marker.getPosition());
    },

    mapError(error){
      console.log('code: ' + error.code + '\n' +
      'message: ' + error.message + '\n');
    }
  },
  getters: {
    Position({ state }){
      return state.position;
    },
    Container({ state }) {
      return state.container;
    }
  },
});

export default geolocation;
