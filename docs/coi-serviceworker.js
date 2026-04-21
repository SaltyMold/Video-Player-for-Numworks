/*
Cross-Origin-Isolation Service Worker
Enables SharedArrayBuffer for multi-threaded FFmpeg WASM
*/

self.addEventListener('install', () => self.skipWaiting())
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()))

self.addEventListener('fetch', (e) => {
  if (e.request.cache === 'only-if-cached' && e.request.mode !== 'same-origin') {
    return
  }
  
  // Add COEP header to responses
  e.respondWith(
    fetch(e.request).then((response) => {
      if (!response || response.status !== 200 || response.type === 'error') {
        return response
      }
      
      const newResponse = response.clone()
      const clonedResponse = new Response(newResponse.body, newResponse)
      clonedResponse.headers.append('Cross-Origin-Embedder-Policy', 'require-corp')
      clonedResponse.headers.append('Cross-Origin-Opener-Policy', 'same-origin')
      return clonedResponse
    }).catch(() => {
      return fetch(e.request)
    })
  )
})
