(function() {
    'use strict';

    angular
        .module('vertoControllers')
        .controller('LoadingController', ['$rootScope', '$scope', '$location',
            function($rootScope, $scope, $location) {
                console.log('Loading controller');
                $rootScope.$on('res_check_done', function() {
                    $location.path('/preview');
                });
            }
        ]);
})();
