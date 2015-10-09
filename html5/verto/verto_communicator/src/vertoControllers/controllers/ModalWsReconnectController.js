(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalWsReconnectController', ModalWsReconnectController);

    ModalWsReconnectController.$inject = ['$scope', 'storage', 'verto'];

    function ModalWsReconnectController($scope, storage, verto) {
      console.debug('Executing ModalWsReconnectController'); 
    };
                
                
})();
