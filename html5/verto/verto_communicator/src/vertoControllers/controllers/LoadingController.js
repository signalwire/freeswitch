(function() {
    'use strict';

    angular
        .module('vertoControllers')
        .controller('LoadingController', ['$rootScope', '$scope', '$location', '$interval', 'verto',
            function($rootScope, $scope, $location, $interval, verto) {
                console.log('Loading controller');
                $interval(function() {
                    if (verto.data.resCheckEnded) {
                      $location.path('/preview');
                    }
                }, 1000);
            }
        ]);
})();
