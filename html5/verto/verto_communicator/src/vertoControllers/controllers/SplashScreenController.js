(function() {
  'use strict';

  angular
  .module('vertoControllers')
  .controller('SplashScreenController', ['$scope', '$rootScope', '$location', '$timeout', 'storage', 'splashscreen', 'prompt', 'verto',
    function($scope, $rootScope, $location, $timeout, storage, splashscreen, prompt, verto) {
      console.debug('Executing SplashScreenController.');

      $scope.progress_percentage = splashscreen.progress_percentage;
      $scope.message = '';
      $scope.interrupt_next = false;
      $scope.errors = [];

      var redirectTo = function(link, activity) {
        if(activity) {
          if(activity == 'browser-upgrade') {
            link = activity;
          }
        }

        $location.path(link);
      }

      var checkProgressState = function(current_progress, status, promise, activity, soft, interrupt, message) {
        $scope.progress_percentage = splashscreen.calculate(current_progress);
        $scope.message = message;

        if(interrupt && status == 'error') {
          $scope.errors.push(message);
          if(!soft) {
            redirectTo('', activity);
            return;
          } else {
            message = message + '. Continue?';
          };

          if(!confirm(message)) {
            $scope.interrupt_next = true;
          };
        };

        if($scope.interrupt_next) {
          return;
        };

        $scope.message = splashscreen.getProgressMessage(current_progress+1);

        return true;
      };

      $rootScope.$on('progress.next', function(ev, current_progress, status, promise, activity, soft, interrupt, message) {
        $timeout(function() {
          if(promise) {
            promise.then(function(response) {
              message = response['message'];
              status = response['status'];
              if(checkProgressState(current_progress, status, promise, activity, soft, interrupt, message)) {
                splashscreen.next();
              };
            });

            return;
          }

          if(!checkProgressState(current_progress, status, promise, activity, soft, interrupt, message)) {
            return;
          }

          splashscreen.next();
        }, 400);
      });

      $rootScope.$on('progress.complete', function(ev, current_progress) {
        $scope.message = 'Complete';
        if(verto.data.connected) {
          if (storage.data.preview) {
            $location.path('/preview');
          }
          else {
            $location.path('/dialpad');
          }
        } else {
          redirectTo('/login');
          $location.path('/login');
        }
      });

      splashscreen.next();

    }]);

})();
