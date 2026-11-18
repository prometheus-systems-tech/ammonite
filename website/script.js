/* Click a screenshot to see it larger, floating above the page. */
(function () {
  var shots = document.querySelectorAll('.shot img');
  if (!shots.length) return;

  var overlay = document.createElement('div');
  overlay.className = 'lightbox';
  overlay.setAttribute('role', 'dialog');
  overlay.setAttribute('aria-modal', 'true');
  overlay.setAttribute('aria-label', 'Screenshot, enlarged');
  overlay.hidden = true;
  overlay.innerHTML =
    '<button class="lightbox-close" type="button" aria-label="Close">&times;</button>' +
    '<figure><img alt=""><figcaption></figcaption></figure>';
  document.body.appendChild(overlay);

  var big = overlay.querySelector('img');
  var cap = overlay.querySelector('figcaption');
  var closeBtn = overlay.querySelector('.lightbox-close');
  var opener = null;

  function open(img) {
    opener = img;
    big.src = img.currentSrc || img.src;
    big.alt = img.alt;
    var fc = img.parentNode.querySelector('figcaption');
    cap.textContent = fc ? fc.textContent : img.alt;
    overlay.hidden = false;
    document.documentElement.classList.add('lightbox-open');
    closeBtn.focus();
  }

  function close() {
    overlay.hidden = true;
    big.removeAttribute('src');
    document.documentElement.classList.remove('lightbox-open');
    if (opener) opener.focus();
  }

  shots.forEach(function (img) {
    img.tabIndex = 0;
    img.setAttribute('role', 'button');
    img.setAttribute('aria-label', img.alt + ' (click to enlarge)');
    img.addEventListener('click', function () { open(img); });
    img.addEventListener('keydown', function (e) {
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); open(img); }
    });
  });

  overlay.addEventListener('click', function (e) {
    if (e.target !== big) close();
  });
  document.addEventListener('keydown', function (e) {
    if (!overlay.hidden && e.key === 'Escape') close();
    if (!overlay.hidden && e.key === 'Tab') { e.preventDefault(); closeBtn.focus(); }
  });
})();
