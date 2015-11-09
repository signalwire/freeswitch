(function() {
  'use strict';

  angular
    .module('vertoControllers')
	.controller('MenuController', ['$scope', '$http', '$location',
	  'verto', 'storage', '$rootScope',
	  function($scope, $http, $location, verto, storage, $rootScope) {
	    console.debug('Executing MenuController.');
      $scope.storage = storage;

      $rootScope.$on('testSpeed', function(e, data) {
        var dedEncWatermark = storage.data.dedEncWatermark;
        var vidQual = storage.data.vidQual;

        $scope.bandDown = data.downKPS;
        $scope.bandUp = data.upKPS;
        $scope.dedEnc = storage.data.useDedenc;

        $scope.iconClass = 'mdi-device-signal-wifi-3-bar';

        if ($scope.bandDown < dedEncWatermark) {
          $scope.iconClass = 'mdi-device-signal-wifi-1-bar dedenc';
        } else if ($scope.bandDown >= 2*dedEncWatermark) {
          $scope.iconClass = 'mdi-device-signal-wifi-4-bar';
        }

        verto.videoQuality.forEach(function(vid) {
          if (vid.id == vidQual){
            $scope.vidRes = vid.label;
          }
        });

        $scope.$apply();
      });
    }
	]);

})();
