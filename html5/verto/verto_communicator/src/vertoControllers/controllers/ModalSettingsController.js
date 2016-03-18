(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('ModalSettingsController', ['$scope', '$http',
      '$location', '$modalInstance', '$rootScope', 'storage', 'verto', 'toastr',
      function($scope, $http, $location, $modalInstance, $rootScope, storage, verto, toastr) {
        console.debug('Executing ModalSettingsController.');

        $scope.storage = storage;
        $scope.verto = verto;
        $scope.mydata = angular.copy(storage.data);

        $scope.speakerFeature = typeof document.getElementById('webcam').sinkId !== 'undefined';

        $scope.ok = function() {
          if ($scope.mydata.selectedSpeaker != storage.data.selectedSpeaker) {
            $rootScope.$emit('changedSpeaker', $scope.mydata.selectedSpeaker);
          }
          storage.changeData($scope.mydata);
          verto.data.instance.iceServers(storage.data.useSTUN);

          if (storage.data.autoBand) {
            $scope.testSpeed();
          }
          $modalInstance.close('Ok.');
        };

        $scope.cancel = function() {
          $modalInstance.dismiss('cancel');
        };

        $scope.refreshDeviceList = function() {
          return verto.refreshDevices();
        };

        $scope.showPreview = function() {
         $modalInstance.close('Ok.');
         if (!verto.data.call) {
           $location.path('/preview');
           return;
         }
         else {
           toastr.warning('Can\'t display preview settings during a call');
         }
        };

        $scope.testSpeed = function() {
          return verto.testSpeed(cb);

          function cb(data) {
            $scope.mydata.vidQual = storage.data.vidQual;
            $scope.speedMsg = 'Up: ' + data.upKPS + ' Down: ' + data.downKPS;
            $scope.$apply();
          }
        };

        $scope.resetSettings = function() {
	  if (confirm('Factory Reset Settings?')) {
            storage.factoryReset();
            $scope.logout();
            $modalInstance.close('Ok.');
	    window.location.reload();
	  };
        };

        $scope.checkAutoBand = function(option) {
          $scope.mydata.useDedenc = false;
          var bestres = videoQuality[videoQuality.length-1];
          $scope.mydata.vidQual = bestres.id;
          storage.data.vidQual = bestres.id;
          verto.data.instance.videoParams({
            minWidth: bestres.width,
            minHeight: bestres.height,
            maxWidth: bestres.width,
            maxHeight: bestres.height,
            minFrameRate: 15,
            vertoBestFrameRate: storage.data.bestFrameRate
          });
          storage.data.vidQual = bestres.id;
          if (!option) {
            $scope.mydata.outgoingBandwidth = 'default';
            $scope.mydata.incomingBandwidth = 'default';
            $scope.mydata.testSpeedJoin = false;

          } else {
            $scope.mydata.testSpeedJoin = true;
          }
        };

        $scope.checkUseDedRemoteEncoder = function(option) {
          if (['0', 'default', '5120'].indexOf(option) != -1) {
            $scope.mydata.useDedenc = false;
          } else {
            $scope.mydata.useDedenc = true;
          }
        };

        $scope.checkVideoQuality = function(resolution) {
          var w = videoResolution[resolution]['width'];
          var h = videoResolution[resolution]['height'];
          storage.data.vidQual = resolution;
          verto.data.instance.videoParams({
            minWidth: w,
            minHeight: h,
            maxWidth: w,
            maxHeight: h,
            minFrameRate: 15,
            vertoBestFrameRate: storage.data.bestFrameRate
          });

        };

      }
    ]);

})();
