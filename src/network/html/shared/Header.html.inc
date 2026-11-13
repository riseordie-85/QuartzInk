<header class="qz-head">
  <div class="qz-bar">
    <a class="qz-wordmark" href="/"><b>Quartz</b> Ink</a>
    <span class="qz-page-title" id="qzPageTitle"></span>
  </div>
  <nav class="qz-nav" aria-label="Pages">
    <a href="/">Home</a>
    <a href="/files">Files</a>
    <a href="/wallpapers">Wallpapers</a>
    <a href="/fonts">Fonts</a>
    <a href="/stats">Stats</a>
    <a href="/settings">Settings</a>
    <a href="/plugins">Plugins</a>
  </nav>
  <script>
    (function () {
      // The server sets cpPlugins=1 on every page while plugins are enabled.
      if (!document.cookie.split('; ').includes('cpPlugins=1')) {
        document.querySelectorAll('.qz-nav a[href="/plugins"]').forEach((a) => a.remove());
      }
      const titles = {
        '/': '', '/files': 'Files', '/wallpapers': 'Wallpapers', '/fonts': 'Fonts', '/stats': 'Reading Stats',
        '/settings': 'Settings', '/plugins': 'Plugins', '/plugins-run': 'Plugins',
      };
      const path = location.pathname.replace(/\/+$/, '') || '/';
      if (path === '/') document.body.classList.add('qz-home');
      document.getElementById('qzPageTitle').textContent = titles[path] || '';
      const navPath = path === '/plugins-run' ? '/plugins' : path;
      const active = document.querySelector('.qz-nav a[href="' + navPath + '"]');
      if (active) {
        active.classList.add('active');
        active.setAttribute('aria-current', 'page');
        if (active.scrollIntoView) active.scrollIntoView({ block: 'nearest', inline: 'center' });
      }
    })();
  </script>
</header>
