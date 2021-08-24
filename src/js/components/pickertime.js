
let datetimePicker = {
  timeConfig(selector){
    let currentDate = new Date();
    let hour = currentDate.getHours();
    let min = currentDate.getMinutes();

    hour = (hour < 10) ? '0' + hour : hour;
    min = (min < 10) ? '0'+ min : min;

    return {
      inputEl: selector,
      toolbar: true,
      toolbarCloseText: 'Cerrar',
      rotateEffect: true,
      openIn: 'sheet',
      value: [ hour, min ],
      formatValue: function (values) {
        return values[0] + ':' + values[1] + ':00';
      },
      cols: [
        // Hours
        {
          values: (function () {
            var arr = [];
            for (var i = 0; i <= 23; i++) { arr.push(i < 10 ? '0' + i : i); }
              return arr;
          })(),
        },
        // Divider
        {
          divider: true,
          content: ':'
        },
        // Minutes
        {
          values: (function () {
            var arr = [];
            for (var i = 0; i <= 59; i++) { arr.push(i < 10 ? '0' + i : i); }
              return arr;
          })(),
        }
      ],
    };
  },
  dateConfig(selector){
    const today = new Date();
    return{
      inputEl: selector,
      toolbar: true,
      toolbarCloseText: 'Cerrar',
      rotateEffect: true,
      openIn: "sheet",
      value: [
        today.getMonth(),
        today.getDate(),
        today.getFullYear(),
        today.getHours() < 10 ? '0' + today.getHours() : today.getHours(),
        today.getMinutes() < 10 ? '0' + today.getMinutes() : today.getMinutes()
      ],
      formatValue: function (values, displayValues) {
        let day = values[1];
        let month = values[0];
        month++;
        month = (month < 10) ? "0" + month : month;
        day = (day < 10) ? "0" + day : day;
        return values[2] + '-' + month + '-' + day + ' ' + values[3] + ':' + values[4] + ':00';
      },
      cols: [
        // Months
        {
          values: ('0 1 2 3 4 5 6 7 8 9 10 11').split(' '),
          displayValues: ('Enero Febrero Marzo Abril Mayo Junio Julio Agosto Septiembre Octubre Noviembre Diciembre').split(' '),
          textAlign: 'left'
        },
        // Days
        {
          values: [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31],
        },
        // Years
        {
          values: (function () {
            var arr = [];
            for (var i = 1950; i <= 2030; i++) { arr.push(i); }
              return arr;
          })(),
        },
        // Space divider
        {
          divider: true,
          content: '&nbsp;&nbsp;'
        },
        // Hours
        {
          values: (function () {
            var arr = [];
            for (var i = 0; i <= 23; i++) { arr.push(i < 10 ? '0' + i : i); }
              return arr;
          })(),
        },
        // Divider
        {
          divider: true,
          content: ':'
        },
        // Minutes
        {
          values: (function () {
            var arr = [];
            for (var i = 0; i <= 59; i++) { arr.push(i < 10 ? '0' + i : i); }
              return arr;
          })(),
        }
      ],
      on: {
        change: function (picker, values, displayValues) {
          var daysInMonth = new Date(picker.value[2], picker.value[0]*1 + 1, 0).getDate();
          if (values[1] > daysInMonth) {
            picker.cols[1].setValue(daysInMonth);
          }
        },
      }
    };
  },
};

export default datetimePicker;
