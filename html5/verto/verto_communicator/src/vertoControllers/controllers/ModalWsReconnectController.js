(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalWsReconnectController', ModalWsReconnectController);

    ModalWsReconnectController.$inject = ['$rootScope', '$scope', 'storage', 'verto'];

    function ModalWsReconnectController($rootScope, $scope, storage, verto) {
      console.debug('Executing ModalWsReconnectController'); 

      $scope.closeReconnect = closeReconnect;

      function closeReconnect() {
        if ($rootScope.ws_modalInstance && verto.data.instance) {
          verto.data.instance.rpcClient.stopRetrying();
          $rootScope.ws_modalInstance.close();
          delete verto.data.instance;
        }
      };
    };
                
                
})();
