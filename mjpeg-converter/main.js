import './style.css'
import { FFmpeg } from '@ffmpeg/ffmpeg'
import { fetchFile, toBlobURL } from '@ffmpeg/util'

// --- DOM refs ---
const fileInput     = document.getElementById('fileInput')
const dropzone      = document.getElementById('dropzone')
const fileInfo      = document.getElementById('fileInfo')
const fileName      = document.getElementById('fileName')
const fileSize      = document.getElementById('fileSize')
const convertBtn    = document.getElementById('convertBtn')
const progressWrap  = document.getElementById('progressWrap')
const progressLabel = document.getElementById('progressLabel')
const progressPct   = document.getElementById('progressPct')
const progressFill  = document.getElementById('progressFill')
const logBox        = document.getElementById('logBox')
const errorBox      = document.getElementById('errorBox')
const resultBox     = document.getElementById('resultBox')
const outSize       = document.getElementById('outSize')
const downloadLink  = document.getElementById('downloadLink')
const paramQv        = document.getElementById('paramQv')
const paramFps       = document.getElementById('paramFps')
const paramDuration  = document.getElementById('paramDuration')
const paramCrop      = document.getElementById('paramCrop')
const paramMultithread = document.getElementById('paramMultithread')
const prevQv         = document.getElementById('prevQv')
const prevVf         = document.getElementById('prevVf')
const prevDuration   = document.getElementById('prevDuration')
const prevInputFile  = document.getElementById('prevInputFile')
const prevOutputFile = document.getElementById('prevOutputFile')
const downloadFileName = document.getElementById('downloadFileName')
const resFps         = document.getElementById('resFps')
const resDuration    = document.getElementById('resDuration')
const previewCanvas  = document.getElementById('previewCanvas')

// --- State ---
let selectedFile = null
let selectedFileName = 'input.mp4'
let ffmpeg = null
let conversionInProgress = false

// --- MJPEG animation ---
function playMJPEGPreview(blob, fps = 15) {
  const canvas = previewCanvas
  const ctx = canvas.getContext('2d')
  
  // Parse MJPEG to extract individual JPEG frames
  const reader = new FileReader()
  reader.onload = (e) => {
    const buffer = e.target.result
    const bytes = new Uint8Array(buffer)
    
    // Find JPEG frames (start with FFD8, end with FFD9)
    const frames = []
    let i = 0
    while (i < bytes.length - 1) {
      if (bytes[i] === 0xFF && bytes[i + 1] === 0xD8) {
        const frameStart = i
        // Find end marker
        for (let j = i + 2; j < bytes.length - 1; j++) {
          if (bytes[j] === 0xFF && bytes[j + 1] === 0xD9) {
            frames.push(bytes.slice(frameStart, j + 2))
            i = j + 2
            break
          }
        }
      } else {
        i++
      }
    }
    
    console.log(`✓ Found ${frames.length} JPEG frames in MJPEG (fps=${fps})`)
    
    if (frames.length === 0) {
      console.warn('No frames found in MJPEG')
      return
    }
    
    // Animate frames with precise timing
    let frameIndex = 0
    const frameDuration = 1000 / fps  // milliseconds per frame
    const startTime = performance.now()
    
    const animateFrame = (currentTime) => {
      // Calculate which frame should be displayed based on elapsed time
      const elapsed = currentTime - startTime
      const targetFrameIndex = Math.floor((elapsed / frameDuration) % frames.length)
      
      // Draw the frame
      const blob = new Blob([frames[targetFrameIndex]], { type: 'image/jpeg' })
      const url = URL.createObjectURL(blob)
      
      const img = new Image()
      img.onload = () => {
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height)
        URL.revokeObjectURL(url)
      }
      img.src = url
      
      // Continue animation
      requestAnimationFrame(animateFrame)
    }
    
    requestAnimationFrame(animateFrame)
  }
  reader.readAsArrayBuffer(blob)
}
function formatBytes(b) {
  if (b < 1024) return b + ' B'
  if (b < 1024 * 1024) return (b / 1024).toFixed(1) + ' KB'
  return (b / (1024 * 1024)).toFixed(2) + ' MB'
}

function secToHMS(s) {
  s = Math.max(1, Math.floor(s))
  const h   = String(Math.floor(s / 3600)).padStart(2, '0')
  const m   = String(Math.floor((s % 3600) / 60)).padStart(2, '0')
  const sec = String(s % 60).padStart(2, '0')
  return `${h}:${m}:${sec}`
}

function clamp(val, min, max) { return Math.max(min, Math.min(max, val)) }

function getParams() {
  return {
    qv:       clamp(parseInt(paramQv.value)       || 24, 2,    31),
    fps:      clamp(parseInt(paramFps.value)      || 15, 1,    60),
    duration: clamp(parseInt(paramDuration.value) || 30, 1, 3600),
    crop:     paramCrop.checked,
    multithread: paramMultithread.checked,
  }
}

function updatePreview() {
  const p = getParams()
  prevQv.textContent       = p.qv
  prevDuration.textContent = secToHMS(p.duration)
  
  // Update input/output filenames
  prevInputFile.textContent = selectedFileName
  const outputName = selectedFileName.substring(0, selectedFileName.lastIndexOf('.')) + '.mjpeg'
  prevOutputFile.textContent = outputName
  downloadFileName.textContent = outputName
  
  // Build video filter string
  const vf = p.crop
    ? `scale=320:240:force_original_aspect_ratio=increase,crop=320:240,setsar=1:1,fps=${p.fps}`
    : `scale=320:240,setsar=1:1,fps=${p.fps}`
  prevVf.textContent = vf
}

function addLog(msg, type = '') {
  const line = document.createElement('div')
  line.className = 'log-line ' + type
  line.textContent = msg
  logBox.appendChild(line)
  logBox.scrollTop = logBox.scrollHeight
}

function setProgress(label, pct = null) {
  progressLabel.textContent = label
  if (pct !== null) {
    progressPct.textContent = pct + '%'
    progressFill.classList.remove('indeterminate')
    progressFill.style.width = pct + '%'
  } else {
    progressPct.textContent = '—'
    progressFill.classList.add('indeterminate')
    progressFill.style.width = ''
  }
}

function showError(msg) {
  errorBox.textContent = '⚠ ' + msg
  errorBox.classList.add('visible')
}

function handleFile(file) {
  if (!file) return
  selectedFile = file
  selectedFileName = file.name
  fileName.textContent = file.name
  fileSize.textContent = formatBytes(file.size)
  fileInfo.classList.add('visible')
  convertBtn.disabled = false
  errorBox.classList.remove('visible')
  resultBox.classList.remove('visible')
  updatePreview()
}

// --- Events ---
;[paramQv, paramFps, paramDuration].forEach(el => el.addEventListener('input', updatePreview))

// Handle switch toggle
function updateSwitchUI() {
  const switchWrap = paramCrop.closest('.switch-wrap')
  if (paramCrop.checked) {
    switchWrap.classList.add('crop-mode')
    switchWrap.classList.remove('scale-mode')
  } else {
    switchWrap.classList.remove('crop-mode')
    switchWrap.classList.add('scale-mode')
  }
  updatePreview()
}

function updateMultithreadUI() {
  const switchWrap = paramMultithread.closest('.switch-wrap')
  if (paramMultithread.checked) {
    switchWrap.classList.add('multi-mode')
    switchWrap.classList.remove('mono-mode')
  } else {
    switchWrap.classList.remove('multi-mode')
    switchWrap.classList.add('mono-mode')
  }
  updatePreview()
}

paramCrop.addEventListener('change', updateSwitchUI)
paramMultithread.addEventListener('change', updateMultithreadUI)

// Make crop switch toggle clickable
const switchToggleCrop = paramCrop.closest('.switch-container').querySelector('.switch-toggle')
if (switchToggleCrop) {
  switchToggleCrop.addEventListener('click', (e) => {
    console.log('Crop switch toggle clicked')
    e.preventDefault()
    paramCrop.checked = !paramCrop.checked
    paramCrop.dispatchEvent(new Event('change'))
  })
}

// Make multithread switch toggle clickable
const switchToggleMulti = paramMultithread.closest('.switch-container').querySelector('.switch-toggle')
if (switchToggleMulti) {
  switchToggleMulti.addEventListener('click', (e) => {
    console.log('Multithread switch toggle clicked')
    e.preventDefault()
    paramMultithread.checked = !paramMultithread.checked
    paramMultithread.dispatchEvent(new Event('change'))
  })
}

updateSwitchUI()
updateMultithreadUI()

fileInput.addEventListener('change', e => handleFile(e.target.files[0]))

dropzone.addEventListener('dragover', e => { e.preventDefault(); dropzone.classList.add('drag-over') })
dropzone.addEventListener('dragleave', () => dropzone.classList.remove('drag-over'))
dropzone.addEventListener('drop', e => {
  e.preventDefault()
  dropzone.classList.remove('drag-over')
  const file = e.dataTransfer.files[0]
  if (file?.type.startsWith('video/')) handleFile(file)
  else showError('Veuillez déposer un fichier vidéo.')
})

// --- FFmpeg load ---
async function loadFFmpeg() {
  ffmpeg = new FFmpeg()

  let progressListener = null
  
  ffmpeg.on('log', ({ message }) => {
    // Filter out verbose FFmpeg stats and debug messages
    if (message.match(/^(video|audio|subtitle|other streams|global headers|muxing overhead):/i) ||
        message.match(/^\s+/) || // Skip indented lines (verbose output)
        message.includes('No file format') ||
        message.includes('Unknown encoder')) {
      return
    }
    addLog(message)
  })
  
  progressListener = ({ progress }) => {
    // Only update progress if conversion is in progress
    if (conversionInProgress) {
      const pct = Math.min(Math.round(progress * 100), 99)
      setProgress('Conversion en cours…', pct)
    }
  }
  ffmpeg.on('progress', progressListener)
  
  // Store listener for later removal
  ffmpeg._progressListener = progressListener

  // Check if SharedArrayBuffer is available and user wants multi-threading
  const p = getParams()
  const canUseMultithread = typeof SharedArrayBuffer !== 'undefined'
  const wantMultithread = p.multithread
  const useMultithread = canUseMultithread && wantMultithread
  
  let baseURL, loadConfig
  if (useMultithread) {
    // Multi-thread available and requested
    addLog('Multi-thread activé → core multi-thread', 'active')
    baseURL = 'https://cdn.jsdelivr.net/npm/@ffmpeg/core-mt@0.12.6/dist/esm'
    loadConfig = {
      coreURL:   await toBlobURL(`${baseURL}/ffmpeg-core.js`,        'text/javascript'),
      wasmURL:   await toBlobURL(`${baseURL}/ffmpeg-core.wasm`,      'application/wasm'),
      workerURL: await toBlobURL(`${baseURL}/ffmpeg-core.worker.js`, 'text/javascript'),
    }
  } else if (wantMultithread && !canUseMultithread) {
    // Multi-thread requested but not available
    addLog('Multi-thread demandé mais indisponible → fallback mono-thread', 'active')
    baseURL = 'https://cdn.jsdelivr.net/npm/@ffmpeg/core@0.12.6/dist/esm'
    loadConfig = {
      coreURL: await toBlobURL(`${baseURL}/ffmpeg-core.js`, 'text/javascript'),
      wasmURL: await toBlobURL(`${baseURL}/ffmpeg-core.wasm`, 'application/wasm'),
    }
  } else {
    // Mono-thread (default)
    addLog('Mono-thread sélectionné → core mono-thread', 'active')
    baseURL = 'https://cdn.jsdelivr.net/npm/@ffmpeg/core@0.12.6/dist/esm'
    loadConfig = {
      coreURL: await toBlobURL(`${baseURL}/ffmpeg-core.js`, 'text/javascript'),
      wasmURL: await toBlobURL(`${baseURL}/ffmpeg-core.wasm`, 'application/wasm'),
    }
  }

  // Load with timeout
  const loadPromise = ffmpeg.load(loadConfig)
  const timeoutPromise = new Promise((_, reject) => 
    setTimeout(() => reject(new Error('FFmpeg load timeout after 30s')), 30000)
  )
  
  await Promise.race([loadPromise, timeoutPromise])
}

// --- Convert ---
convertBtn.addEventListener('click', async () => {
  if (!selectedFile) return
  const p = getParams()

  conversionInProgress = true
  convertBtn.disabled = true
  errorBox.classList.remove('visible')
  resultBox.classList.remove('visible')
  progressWrap.classList.add('visible')
  logBox.innerHTML = ''

  try {
    if (!ffmpeg) {
      setProgress('Chargement de FFmpeg WASM…')
      addLog('Chargement du core multi-thread…', 'active')
      await loadFFmpeg()
      addLog('FFmpeg chargé ✓', 'done')
    }

    setProgress('Lecture du fichier…')
    addLog(`Source : ${selectedFile.name} (${formatBytes(selectedFile.size)})`, 'active')
    addLog(`Params → q:v=${p.qv}  fps=${p.fps}  durée=${secToHMS(p.duration)}  mode=${p.crop ? 'crop' : 'scale'}`, 'active')

    const ext = selectedFile.name.substring(selectedFile.name.lastIndexOf('.'))
    const inputName = 'input' + ext
    const baseName = selectedFile.name.substring(0, selectedFile.name.lastIndexOf('.'))
    const outputName = baseName + '.mjpeg'

    await ffmpeg.writeFile(inputName, await fetchFile(selectedFile))

    setProgress('Conversion en cours…', 0)
    addLog('Lancement MJPEG…', 'active')

    try {
      // Try to delete any previous output file
      await ffmpeg.deleteFile(outputName)
    } catch (e) {
      // File might not exist, ignore
    }

    // FFmpeg.wasm can hang, so run with timeout
    const vf = p.crop
      ? `scale=320:240:force_original_aspect_ratio=increase,crop=320:240,setsar=1:1,fps=${p.fps}`
      : `scale=320:240,setsar=1:1,fps=${p.fps}`
    
    const execPromise = ffmpeg.exec([
      '-i', inputName,
      '-vf', vf,
      '-t', secToHMS(p.duration),
      '-vcodec', 'mjpeg',
      '-q:v', String(p.qv),
      '-an',
      outputName,
    ])

    // Also watch for the output file appearing — some FFmpeg WASM builds
    // may not resolve `exec` properly even though output was produced.
    const timeoutPromise = new Promise((resolve) => {
      setTimeout(() => {
        console.warn('⚠ FFmpeg appears stuck, continuing anyway')
        resolve('timeout')
      }, 120000) // 120 seconds timeout
    })

    const outputWaitPromise = (async () => {
      const start = Date.now()
      const maxWait = 60000
      while (Date.now() - start < maxWait) {
        try {
          const data = await ffmpeg.readFile(outputName)
          if (data && data.length > 0) return 'output_ready'
        } catch (e) {
          // file not yet present, keep polling
        }
        // small delay between polls
        await new Promise(r => setTimeout(r, 500))
      }
      return 'output_timeout'
    })()

    const result = await Promise.race([execPromise, timeoutPromise, outputWaitPromise])
    if (result === 'timeout' || result === 'output_timeout') {
      addLog('⚠ Conversion longue, passage à la finalisation…', 'active')
    }

    console.log('✓ FFmpeg exec completed or timeout')

    // Stop progress updates to avoid races with the final UI state
    conversionInProgress = false

    // Try to remove progress listener if supported by this FFmpeg build
    if (ffmpeg._progressListener && typeof ffmpeg.off === 'function') {
      try {
        ffmpeg.off('progress', ffmpeg._progressListener)
      } catch (e) {
        console.warn('ffmpeg.off failed:', e)
      }
    }
    ffmpeg._progressListener = null

    // Force UI to 100%
    progressFill.style.width = '100%'
    progressPct.textContent = '100%'
    progressLabel.textContent = 'Finalisation…'
    
    addLog('✓ Récupération du fichier…', 'done')
    console.log('✓ Progress set to 100%')

    // Read the output file with timeout and fallback
    console.log('Reading output file...')
    let data = null
    try {
      const readPromise = ffmpeg.readFile(outputName)
      const readTimeout = new Promise((_, reject) => setTimeout(() => reject(new Error('read_timeout')), 30000))
      data = await Promise.race([readPromise, readTimeout])
      if (!data || data.length === 0) throw new Error('empty_output')
      console.log('✓ File read:', data.length, 'bytes')
      addLog(`✓ Fichier lu: ${data.length} bytes`, 'active')
    } catch (readErr) {
      console.warn('ffmpeg.readFile failed or timed out:', readErr)
      // Try low-level FS fallback if available
      try {
        if (typeof ffmpeg.FS === 'function') {
          console.log('Trying ffmpeg.FS fallback...')
          data = ffmpeg.FS('readFile', outputName)
          if (data && data.length > 0) {
            console.log('✓ FS fallback read:', data.length, 'bytes')
            addLog(`✓ Fallback fichier lu: ${data.length} bytes`, 'active')
          }
        }
      } catch (fsErr) {
        console.warn('FFmpeg FS fallback failed:', fsErr)
      }
    }

    if (!data || data.length === 0) {
      showError('Impossible de lire le fichier de sortie (' + outputName + ')')
      addLog('Erreur : lecture du fichier de sortie impossible', 'err')
      throw new Error('No output data')
    }
    
    const blob = new Blob([data], { type: 'video/x-motion-jpeg' })
    console.log('✓ Blob created:', blob.size, 'bytes')
    addLog(`✓ Blob créé: ${blob.size} bytes`, 'active')
    
    const url = URL.createObjectURL(blob)
    console.log('✓ Object URL created')
    addLog('✓ URL d\'objet créée', 'active')

    // Update download button
    downloadLink.href = url
    const outputFileName = selectedFileName.substring(0, selectedFileName.lastIndexOf('.')) + '.mjpeg'
    downloadLink.download = outputFileName
    
    // Update stats
    outSize.textContent = formatBytes(blob.size)
    resFps.textContent = p.fps
    resDuration.textContent = p.duration
    
    // Show preview video (MJPEG animation)
    playMJPEGPreview(blob, p.fps)
    
    // Show result box
    console.log('Showing result box...')
    resultBox.classList.remove('hidden')
    resultBox.classList.add('visible')
    
    console.log('✓ UI updated, conversion complete')
    
    addLog(`✓ Prêt pour téléchargement: ${formatBytes(blob.size)}`, 'done')
    setProgress('Terminé !', 100)

    await ffmpeg.deleteFile(inputName)
    await ffmpeg.deleteFile(outputName)

  } catch (err) {
    console.error(err)
    showError(err.message || String(err))
    addLog('Erreur : ' + (err.message || String(err)), 'err')
    setProgress('Erreur')
  } finally {
    conversionInProgress = false
    convertBtn.disabled = false
  }
})

// --- Service Worker registration ---
if (window.location.protocol !== 'file:') {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('./coi-serviceworker.js').catch((err) => {
      console.warn('Service Worker registration failed:', err)
    })
  })
}