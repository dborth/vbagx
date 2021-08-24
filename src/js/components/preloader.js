import $$ from 'dom7';

let Preload = {
  Autocomplete(list, selectorText, selectorValue){
    return {
      inputEl: selectorText,
      openIn: 'dropdown',
      valueProperty: 'text',
      textProperty: 'text',
      limit: 8,
      source: function (query, render) {
        let results = [];

        if (query.length === 0) {
          render(results);
          return;
        }

        for (let i = 0; i < list.length; i++) {
          if (list[i].text.toLowerCase().indexOf(query.toLowerCase()) >= 0){
            results.push(list[i]);
          }
        }
        render(results);
      },
      on: {
        change: function (value) { 
          $$(selectorValue).val(value[0].id);
        }
      }
    }
  },
}

export default Preload;
