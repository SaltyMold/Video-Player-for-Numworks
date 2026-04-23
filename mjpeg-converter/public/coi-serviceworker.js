/*
Cross-Origin-Isolation Service Worker
Enables SharedArrayBuffer for multi-threaded FFmpeg WASM
*/

self.addEventListener('install', () => self.skipWaiting())
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()))

self.addEventListener('fetch', (e) => {
  // Handle only-if-cached requests for cross-origin resources
  if (e.request.cache === 'only-if-cached' && e.request.mode !== 'same-origin') {
    return
  }
  
  // Simple pass-through: server headers are already set by Vite
  e.respondWith(fetch(e.request).catch(() => new Response('Network error', { status: 503 })))
})
