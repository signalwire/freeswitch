(function() {
  'use strict';

  angular
    .module('vertoControllers')
    .controller('MainController',
      function($scope, $rootScope, $location, $modal, $timeout, $q, verto, storage, CallHistory, toastr, Fullscreen, prompt, eventQueue) {

      console.debug('Executing MainController.');

      $rootScope.master = $location.search().master;
      if ($location.search().watcher === 'true') {
        $rootScope.watcher = true;
        angular.element(document.body).addClass('watcher');
        var dialpad;
        var extension = dialpad = $location.search().extension;
        var canvasID = $location.search().canvas_id;

        if (dialpad) {
          if (canvasID) {
            dialpad += '-canvas-' + canvasID;
          }
          $rootScope.extension = extension;
          $rootScope.canvasID = canvasID;
          $location.search().autocall = dialpad;
        }
      }

      var myVideo = document.getElementById("webcam");
      $scope.verto = verto;
      $scope.storage = storage;
      $scope.call_history = angular.element("#call_history").hasClass('active');
      $rootScope.chatStatus = angular.element('#wrapper').hasClass('toggled');
      $scope.showReconnectModal = true;
      /**
       * (explanation) scope in another controller extends rootScope (singleton)
       */
      $rootScope.chat_counter = 0;
      $rootScope.activePane = 'members';
      /**
       * The number that will be called.
       * @type {string}
       */
      $rootScope.dialpadNumber = '';

      // If verto is not connected, redirects to login page.
      if (!verto.data.connected) {
        console.debug('MainController: WebSocket not connected. Redirecting to login.');
        $location.path('/');
      }

      $rootScope.$on('config.http.success', function(ev) {
        $scope.login(false);
      });

      $rootScope.$on('changedSpeaker', function(event, speakerId) {
        // This should provide feedback
	//setAudioPlaybackDevice(<id>[,<callback>[,<callback arg>]]);
	// if callback is set it will be called as callback(<bool success/fail>, <device name>, <arg if you supplied it>)
        verto.data.call.setAudioPlaybackDevice(speakerId);
      });

      /**
       * Login the user to verto server and
       * redirects him to dialpad page.
       */
      $scope.login = function(redirect) {
        if(redirect == undefined) {
          redirect = true;
        }
        var connectCallback = function(v, connected) {
          $scope.$apply(function() {
          verto.data.connecting = false;
          if (connected) {
            storage.data.ui_connected = verto.data.connected;
            storage.data.ws_connected = verto.data.connected;
            storage.data.name = verto.data.name;
            storage.data.email = verto.data.email;
            storage.data.login = verto.data.login;
            storage.data.password = verto.data.password;
            if (storage.data.autoBand) {
              verto.testSpeed();
            }

            if (redirect && storage.data.preview) {
              $location.path('/preview');
            } else if (redirect) {
              $location.path('/dialpad');
            }
          }
          });
        };

        verto.data.connecting = true;
        verto.connect(connectCallback);
      };

      /**
       * Logout the user from verto server and
       * redirects him to login page.
       */
      $rootScope.logout = function() {
        var disconnect = function() {
          var disconnectCallback = function(v, connected) {
            console.debug('Redirecting to login page.');
            storage.reset();
			if (typeof gapi !== 'undefined'){
				console.debug(gapi);
				gapi.auth.signOut();
			}
            $location.path('/login');
          };

          if (verto.data.call) {
            verto.hangup();
          }

          $scope.closeChat();
          $scope.showReconnectModal = false;
          verto.disconnect(disconnectCallback);

          verto.hangup();
        };

        if (verto.data.call) {
          prompt({
            title: 'Oops, Active Call in Course.',
            message: 'It seems that you are in a call. Do you want to hang up?'
          }).then(function() {
            disconnect();
          });
        } else {
          disconnect();
        }

      };

      /**
       * Shows a modal with the settings.
       */
      $scope.openModalSettings = function() {
        var modalInstance = $modal.open({
          animation: $scope.animationsEnabled,
          templateUrl: 'partials/modal_settings.html',
          controller: 'ModalSettingsController',
        });

        modalInstance.result.then(
          function(result) {
            console.log(result);
          },
          function() {
            console.info('Modal dismissed at: ' + new Date());
          }
        );

        modalInstance.rendered.then(
          function() {
            jQuery.material.init();
          }
        );
      };

      $rootScope.openModal = function(templateUrl, controller, _options) {
        var options = {
          animation: $scope.animationsEnabled,
          templateUrl: templateUrl,
          controller: controller,
        };

        angular.extend(options, _options);

        var modalInstance = $modal.open(options);

        modalInstance.result.then(
          function(result) {
            console.log(result);
          },
          function() {
            console.info('Modal dismissed at: ' + new Date());
          }
        );

        modalInstance.rendered.then(
          function() {
            jQuery.material.init();
          }
        );

        return modalInstance;
      };

      $rootScope.$on('ws.close', onWSClose);
      $rootScope.$on('ws.login', onWSLogin);

      var ws_modalInstance;

      function onWSClose(ev, data) {
        if(ws_modalInstance) {
          return;
        };
        var options = {
          backdrop: 'static',
          keyboard: false
        };
          if ($scope.showReconnectModal) {
            ws_modalInstance = $scope.openModal('partials/ws_reconnect.html', 'ModalWsReconnectController', options);
          };
      };

      function onWSLogin(ev, data) {
        if(storage.data.autoBand) {
          verto.testSpeed();
        }
        if(!ws_modalInstance) {
          return;
        };

        ws_modalInstance.close();
        ws_modalInstance = null;
      };

      $scope.showAbout = function() {
        $scope.openModal('partials/about.html', 'AboutController');
      };

      $scope.showContributors = function() {
        $scope.openModal('partials/contributors.html', 'ContributorsController');
      };

      /**
       * Updates the display adding the new number touched.
       *
       * @param {String} number - New touched number.
       */
      $rootScope.dtmf = function(number) {
        $rootScope.dialpadNumber = $scope.dialpadNumber + number;
        if (verto.data.call) {
          verto.dtmf(number);
        }
      };

      /**
       * Removes the last character from the number.
       */
      $rootScope.backspace = function() {
        var number = $rootScope.dialpadNumber;
        var len = number.length;
        $rootScope.dialpadNumber = number.substring(0, len - 1);
      };


      $scope.toggleCallHistory = function() {
        if (!$scope.call_history) {
          angular.element("#call_history").addClass('active');
          angular.element("#call-history-wrapper").addClass('active');
        } else {
          angular.element("#call_history").removeClass('active');
          angular.element("#call-history-wrapper").removeClass('active');
        }
        $scope.call_history = angular.element("#call_history").hasClass('active');
      };

      $scope.toggleChat = function() {
        if ($rootScope.chatStatus && $rootScope.activePane === 'chat') {
          $rootScope.chat_counter = 0;
        }
        angular.element('#wrapper').toggleClass('toggled');
        $rootScope.chatStatus = angular.element('#wrapper').hasClass('toggled');
      };

      $rootScope.openChat = function() {
        $rootScope.chatStatus = false;
        angular.element('#wrapper').removeClass('toggled');
      };

      $scope.closeChat = function() {
        $rootScope.chatStatus = true;
        angular.element('#wrapper').addClass('toggled');
      };

      $scope.goFullscreen = function() {
        if (storage.data.userStatus !== 'connected') {
          return;
        }
        $rootScope.fullscreenEnabled = !Fullscreen.isEnabled();
        if (Fullscreen.isEnabled()) {
          Fullscreen.cancel();
        } else {
          Fullscreen.enable(document.getElementsByTagName('body')[0]);
        }
      };

      $rootScope.$on('call.video', function(event) {
        storage.data.videoCall = true;
      });

      $rootScope.$on('call.hangup', function(event, data) {
        if (Fullscreen.isEnabled()) {
          Fullscreen.cancel();
        }

        if (!$rootScope.chatStatus) {
          angular.element('#wrapper').toggleClass('toggled');
          $rootScope.chatStatus = angular.element('#wrapper').hasClass('toggled');
        }

        $rootScope.dialpadNumber = '';
        console.debug('Redirecting to dialpad page.');
        $location.path('/dialpad');

        try {
          $rootScope.$digest();
        } catch (e) {
          console.log('not digest');
        }
      });

      $rootScope.$on('page.incall', function(event, data) {
        var page_incall = function() {
          return $q(function(resolve, reject) {
            if (storage.data.askRecoverCall) {
              prompt({
                title: 'Oops, Active Call in Course.',
                message: 'It seems you were in a call before leaving the last time. Wanna go back to that?'
              }).then(function() {
                console.log('redirect to incall page');
                $location.path('/incall');
              }, function() {
                storage.data.userStatus = 'connecting';
                verto.hangup();
              });
            } else {
              console.log('redirect to incall page');
              $location.path('/incall');
            }
            resolve();
          });
        };
        eventQueue.events.push(page_incall);
      });

      $scope.$on('event:google-plus-signin-success', function (event,authResult) {
        // Send login to server or save into cookie
        console.log('Google+ Login Success');
	console.log(authResult);
	gapi.client.load('plus', 'v1', gapiClientLoaded);
      });

      function gapiClientLoaded() {
	gapi.client.plus.people.get({userId: 'me'}).execute(handleEmailResponse);
      }

      function handleEmailResponse(resp){
        var primaryEmail;
	for (var i=0; i < resp.emails.length; i++) {
	  if (resp.emails[i].type === 'account') primaryEmail = resp.emails[i].value;
        }
	console.debug("Primary Email: " + primaryEmail );
	console.debug("display name: " + resp.displayName);
	console.debug("imageurl: " + resp.image.url);
	console.debug(resp);
	console.debug(verto.data);
	verto.data.email = primaryEmail;
	verto.data.name = resp.displayName;
	storage.data.name = verto.data.name;
	storage.data.email = verto.data.email;

	$scope.login();
      }

      $scope.$on('event:google-plus-signin-failure', function (event,authResult) {
        // Auth failure or signout detected
        console.log('Google+ Login Failure');
      });

      $rootScope.callActive = function(data, params) {
        verto.data.mutedMic = storage.data.mutedMic;
        verto.data.mutedVideo = storage.data.mutedVideo;

        if (!storage.data.cur_call) {
          storage.data.call_start = new Date();
        }
        storage.data.userStatus = 'connected';
        var call_start = new Date(storage.data.call_start);
        $rootScope.start_time = call_start;

        $timeout(function() {
          $scope.$broadcast('timer-start');
        });
        myVideo.play();
        storage.data.calling = false;

        storage.data.cur_call = 1;

        $location.path('/incall');

        if(params.useVideo) {
          $rootScope.$emit('call.video', 'video');
        }
      };

      $rootScope.$on('call.active', function(event, data, params) {
        $rootScope.callActive(data, params);
      });

      $rootScope.$on('call.calling', function(event, data) {
        storage.data.calling = true;
      });

      $rootScope.$on('call.incoming', function(event, data) {
        console.log('Incoming call from: ' + data);

        storage.data.cur_call = 0;
        $scope.incomingCall = true;
        storage.data.videoCall = false;
        storage.data.mutedVideo = false;
        storage.data.mutedMic = false;

        prompt({
          title: 'Incoming Call',
          message: 'from ' + data
        }).then(function() {
          var call_start = new Date(storage.data.call_start);
          $rootScope.start_time = call_start;
          console.log($rootScope.start_time);

          $scope.answerCall();
          storage.data.called_number = data;
          CallHistory.add(data, 'inbound', true);
          $location.path('/incall');
        }, function() {
          $scope.declineCall();
          CallHistory.add(data, 'inbound', false);
        });
      });

      $scope.hold = function() {
        storage.data.onHold = !storage.data.onHold;
        verto.data.call.toggleHold();
      };

      /**
       * Hangup the current call.
       */
      $scope.hangup = function() {
        if (!verto.data.call) {
          toastr.warning('There is no call to hangup.');
          $location.path('/dialpad');
          return;
        }

        if ($rootScope.watcher) {
          window.close();
          return;
        }

        //var hangupCallback = function(v, hangup) {
        //  if (hangup) {
        //    $location.path('/dialpad');
        //  } else {
        //    console.debug('The call could not be hangup.');
        //  }
        //};
        //
        //verto.hangup(hangupCallback);
        if (verto.data.shareCall) {
          verto.screenshareHangup();
        }

        verto.hangup();

        $location.path('/dialpad');
      };

      $scope.answerCall = function() {
        storage.data.onHold = false;

        verto.data.call.answer({
          useStereo: storage.data.useStereo,
          useCamera: storage.data.selectedVideo,
          useVideo: storage.data.useVideo,
          useMic: storage.data.useMic,
          callee_id_name: verto.data.name,
          callee_id_number: verto.data.login
        });


        $location.path('/incall');
      };

      $scope.declineCall = function() {
        $scope.hangup();
        $scope.incomingCall = false;
      };

      $scope.play = function() {
        var file = $scope.promptInput('Please, enter filename', '', 'File',
          function(file) {
            verto.data.conf.play(file);
            console.log('play file :', file);
          });

      };

      $scope.stop = function() {
        verto.data.conf.stop();
      };

      $scope.record = function() {
        var file = $scope.promptInput('Please, enter filename', '', 'File',
          function(file) {
            verto.data.conf.record(file);
            console.log('recording file :', file);
          });
      };

      $scope.stopRecord = function() {
        verto.data.conf.stopRecord();
      };

      $scope.snapshot = function() {
        var file = $scope.promptInput('Please, enter filename', '', 'File',
          function(file) {
            verto.data.conf.snapshot(file);
            console.log('snapshot file :', file);
          });
      };


    }
  );

})();
