'use strict';

/* App Module */

var vertoApp = angular.module('vertoApp', [
  'timer',
  'ngRoute',
  'vertoControllers',
  'vertoDirectives',
  'vertoFilters',
  'ngStorage',
  'ngAnimate',
  'toastr',
  'FBAngular',
  'cgPrompt',
  '720kb.tooltips',
  'ui.gravatar',
]);

vertoApp.config(['$routeProvider', 'gravatarServiceProvider', 'toastrConfig',
  function($routeProvider, gravatarServiceProvider, toastrConfig) {
    $routeProvider.
    when('/login', {
      title: 'Login',
      templateUrl: 'partials/login.html',
      controller: 'LoginController'
    }).
    when('/dialpad', {
      title: 'Dialpad',
      templateUrl: 'partials/dialpad.html',
      controller: 'DialPadController'
    }).
    when('/incall', {
        title: 'In a Call',
        templateUrl: 'partials/incall.html',
        controller: 'InCallController'
      }).
      /*when('/contributors', {
        title: 'Contributors',
        templateUrl: 'partials/contributors.html',
        controller: 'ContributorsController',
      }).*/
    when('/browser-upgrade', {
      title: '',
      templateUrl: 'partials/browser_upgrade.html',
      controller: 'BrowserUpgradeController'
    }).
    otherwise({
      redirectTo: '/login'
    });

    gravatarServiceProvider.defaults = {
      default: 'mm'  // Mystery man as default for missing avatars
    };

    angular.extend(toastrConfig, {
      maxOpened: 4
    });
  }
]);

vertoApp.run(['$rootScope', '$location', 'toastr', 'prompt',
  function($rootScope, $location, toastr, prompt) {
    $rootScope.$on('$routeChangeSuccess', function(event, current, previous) {
      $rootScope.title = current.$$route.title;
    });

    $rootScope.safeProtocol = false;

    if (window.location.protocol == 'https:') {
      $rootScope.safeProtocol = true;
    }

    $rootScope.checkBrowser = function() {
      navigator.getUserMedia = navigator.getUserMedia ||
        navigator.webkitGetUserMedia ||
        navigator.mozGetUserMedia;

      if (!navigator.getUserMedia) {
        $location.path('/browser-upgrade');
      }

    };

    $rootScope.promptInput = function(title, message, label, callback) {
      var ret = prompt({
        title: title,
        message: message,
        input: true,
        label: label
      }).then(function(ret) {
        if (angular.isFunction(callback)) {
          callback(ret);
        }
      }, function() {

      });

    };

  }
]);
