(function() {
    'use strict';

    angular
        .module('vertoControllers')
        .controller('LoadingController', ['$rootScope', '$scope', '$location', '$interval', 'verto',
            function($rootScope, $scope, $location, $interval, verto) {
                console.log('Loading controller');
                var int_id;

                $scope.stopInterval = function() {
                    $interval.cancel(int_id);
                };

                int_id = $interval(function() {
                  if (verto.data.resCheckEnded) {
                    $scope.stopInterval();
                    $location.path('/preview');
                  }
                }, 1000);

            }
        ]);
})();
