(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('BrowserUpgradeController', ['$scope', '$http',
      '$location', 'verto', 'storage', 'Fullscreen',
      function($scope, $http, $location, verto, storage, Fullscreen) {
        console.debug('Executing BrowserUpgradeController.');

      }
    ]);

})();