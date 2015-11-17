function attachSinkId(element, sinkId) {
  if (typeof element.sinkId !== 'undefined') {
    element.setSinkId(sinkId)
      .then(function() {
        console.log('Success, audio output device attached:', sinkId);
      })
      .catch(function(error) {
        var errorMessage = error;
        if (error.name === 'SecurityError') {
            errorMessage = 'You need to use HTTPS for selecting audio output ' +
          'device: ' + error;
        }
        console.error(errorMessage);
      });
  } else {
    console.warn('Browser does not support output device selection.');
  }
}
