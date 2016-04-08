(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('SettingsController', ['$scope', '$http',
      '$location', '$rootScope', 'storage', 'verto', '$translate', 'toastr', 'configLanguages',
      function($scope, $http, $location, $rootScope, storage, verto, $translate, toastr, configLanguages) {
        console.debug('Executing ModalSettingsController.');

        $.material.init();

        $scope.speakerFeature = typeof document.getElementById('webcam').sinkId !== 'undefined';
        $scope.storage = storage;
        $scope.verto = verto;
        $scope.mydata = angular.copy(storage.data);
        $scope.languages = configLanguages.languages;
        $scope.languages.unshift({id: 'browser', name : $translate.instant('BROWSER_LANGUAGE')});
        $scope.mydata.language = storage.data.language || 'browser';

        $rootScope.$on('$translateChangeSuccess', function () {
          $translate('BROWSER_LANGUAGE').then(function (translation) {
          $scope.languages[0].name = translation;
          });
        });

        $rootScope.$on('toggledSettings', function(e, status) {
          if (status) {
            $scope.mydata = angular.copy(storage.data);
          } else {
            $scope.ok();
          }
        });

        $scope.ok = function() {
          if ($scope.mydata.selectedSpeaker != storage.data.selectedSpeaker) {
            $rootScope.$emit('changedSpeaker', $scope.mydata.selectedSpeaker);
          }
          storage.changeData($scope.mydata);
          verto.data.instance.iceServers(storage.data.useSTUN);

          if (storage.data.autoBand) {
            $scope.testSpeed();
          }
        };

        $scope.changedLanguage = function(langKey){
          if (langKey === 'browser'){
           storage.data.language = 'browser';
            var browserlang = $translate.preferredLanguage();
            $translate.use(browserlang).then(
              function(lang) {}, function(fail_lang) {
                $translate.use('en');
               });
          } else {
            $translate.use(langKey);
            storage.data.language = langKey;
          }
        };

        $scope.refreshDeviceList = function() {
          return verto.refreshDevices();
        };

        $scope.showPreview = function() {
          var settingsEl = angular.element(document.querySelector('#settings'));
          settingsEl.toggleClass('toggled');
          if (!verto.data.call) {
            $location.path('/preview');
            return;
          }
          else {
            toastr.warning($translate.instant('MESSAGE_DISPLAY_SETTINGS'));
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
