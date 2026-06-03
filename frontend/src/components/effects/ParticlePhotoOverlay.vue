<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as THREE from 'three'
import gsap from 'gsap'
import { useEffects } from '../../composables/useEffects.js'

const props = defineProps({
  imageUrl: { type: String, default: 'https://picsum.photos/400/400' },
  opacity: { type: Number, default: 0.55 },
})

const { canAnimate, effectDensity } = useEffects()
const canvasRef = ref(null)
const shouldRender = computed(() => canAnimate.value && effectDensity.value > 0)

const vertexShader = `
  uniform float uTime;
  uniform float uProgress;
  uniform float uPixelRatio;
  uniform float uPointSize;

  attribute vec3 aTarget;
  attribute vec3 aColor;
  attribute vec3 aSwirl;
  attribute float aSeed;

  varying vec3 vColor;
  varying float vAlpha;

  mat2 rotate2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
  }

  void main() {
    float progress = smoothstep(0.0, 1.0, uProgress);

    vec3 start = aSwirl;
    start.xz = rotate2d(uTime * 0.18) * start.xz;

    float inwardSpin = (1.0 - progress) * 4.8;
    float spiralAngle = inwardSpin + aSeed * 1.6 + uTime * 0.22;
    vec3 spiralOffset = vec3(
      cos(spiralAngle) * 0.42,
      sin(spiralAngle * 0.7) * 0.18,
      sin(spiralAngle) * 0.42
    );
    spiralOffset *= sin(progress * 3.1415926);

    vec3 finalPosition = mix(start, aTarget, progress) + spiralOffset;

    vec4 modelPosition = modelMatrix * vec4(finalPosition, 1.0);
    vec4 viewPosition = viewMatrix * modelPosition;
    gl_Position = projectionMatrix * viewPosition;

    float depthScale = 1.0 / -viewPosition.z;
    gl_PointSize = uPointSize * uPixelRatio * depthScale * 8.0;

    vColor = aColor;
    vAlpha = mix(0.64, 1.0, progress);
  }
`

const fragmentShader = `
  precision highp float;

  varying vec3 vColor;
  varying float vAlpha;

  void main() {
    vec2 center = gl_PointCoord - vec2(0.5);
    float distanceToCenter = length(center);
    float circle = smoothstep(0.5, 0.18, distanceToCenter);
    float glow = smoothstep(0.5, 0.0, distanceToCenter) * 0.28;

    gl_FragColor = vec4(vColor + glow, circle * vAlpha);

    if (gl_FragColor.a < 0.02) {
      discard;
    }
  }
`

let renderer = null
let scene = null
let camera = null
let particles = null
let geometry = null
let material = null
let frameId = null
let clickTween = null
let assembled = false

const clock = new THREE.Clock()
const progressState = { value: 0 }
const uniforms = {
  uTime: { value: 0 },
  uProgress: { value: 0 },
  uPixelRatio: { value: Math.min(window.devicePixelRatio || 1, 2) },
  uPointSize: { value: 3.1 },
}

const loadImagePixels = (src) => new Promise((resolve) => {
  const image = new Image()
  image.crossOrigin = 'anonymous'

  image.onload = () => {
    const size = 400
    const hiddenCanvas = document.createElement('canvas')
    hiddenCanvas.width = size
    hiddenCanvas.height = size

    const context = hiddenCanvas.getContext('2d', { willReadFrequently: true })
    context.drawImage(image, 0, 0, size, size)

    const imageData = context.getImageData(0, 0, size, size)
    resolve({ width: size, height: size, data: imageData.data })
  }

  image.onerror = () => {
    resolve(createFallbackPixels())
  }

  image.src = src
})

const createFallbackPixels = () => {
  const size = 400
  const hiddenCanvas = document.createElement('canvas')
  hiddenCanvas.width = size
  hiddenCanvas.height = size

  const context = hiddenCanvas.getContext('2d', { willReadFrequently: true })
  const gradient = context.createRadialGradient(
    size * 0.48,
    size * 0.42,
    24,
    size * 0.5,
    size * 0.5,
    size * 0.64
  )

  gradient.addColorStop(0, '#fff5d6')
  gradient.addColorStop(0.34, '#ff8f70')
  gradient.addColorStop(0.72, '#4aa3ff')
  gradient.addColorStop(1, '#14151d')

  context.fillStyle = gradient
  context.fillRect(0, 0, size, size)
  context.fillStyle = 'rgba(255, 255, 255, 0.88)'
  context.beginPath()
  context.arc(size * 0.5, size * 0.5, 86, 0, Math.PI * 2)
  context.fill()

  const imageData = context.getImageData(0, 0, size, size)
  return { width: size, height: size, data: imageData.data }
}

const createParticleGeometry = (imagePixels) => {
  const sampleStep = effectDensity.value < 0.5 ? 6 : 4
  const positions = []
  const targets = []
  const colors = []
  const swirls = []
  const seeds = []
  const photoScale = 4.2
  const halfWidth = imagePixels.width * 0.5
  const halfHeight = imagePixels.height * 0.5

  for (let y = 0; y < imagePixels.height; y += sampleStep) {
    for (let x = 0; x < imagePixels.width; x += sampleStep) {
      const index = (y * imagePixels.width + x) * 4
      const r = imagePixels.data[index]
      const g = imagePixels.data[index + 1]
      const b = imagePixels.data[index + 2]
      const a = imagePixels.data[index + 3]
      const brightness = (r + g + b) / 3

      if (a < 20 || brightness < 8) continue

      const targetX = ((x - halfWidth) / imagePixels.width) * photoScale
      const targetY = (-(y - halfHeight) / imagePixels.height) * photoScale
      const seed = Math.random()
      const angle = seed * Math.PI * 2 + y * 0.035
      const radius = 0.45 + Math.random() * 2.8 + y / imagePixels.height
      const height = (Math.random() - 0.5) * 2.6

      positions.push(0, 0, 0)
      targets.push(targetX, targetY, 0)
      swirls.push(
        Math.cos(angle) * radius,
        height + Math.sin(radius * 2.1) * 0.28,
        Math.sin(angle) * radius
      )
      colors.push(r / 255, g / 255, b / 255)
      seeds.push(seed)
    }
  }

  const nextGeometry = new THREE.BufferGeometry()
  nextGeometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3))
  nextGeometry.setAttribute('aTarget', new THREE.Float32BufferAttribute(targets, 3))
  nextGeometry.setAttribute('aColor', new THREE.Float32BufferAttribute(colors, 3))
  nextGeometry.setAttribute('aSwirl', new THREE.Float32BufferAttribute(swirls, 3))
  nextGeometry.setAttribute('aSeed', new THREE.Float32BufferAttribute(seeds, 1))

  return nextGeometry
}

const resize = () => {
  if (!renderer || !camera) return

  const width = window.innerWidth
  const height = window.innerHeight

  camera.aspect = width / height
  camera.updateProjectionMatrix()

  renderer.setSize(width, height, false)
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2))
  uniforms.uPixelRatio.value = Math.min(window.devicePixelRatio || 1, 2)
}

const animate = () => {
  if (!renderer || !scene || !camera) return

  uniforms.uTime.value = clock.getElapsedTime()

  if (particles) {
    particles.rotation.y = uniforms.uTime.value * 0.08 * (1 - uniforms.uProgress.value)
  }

  renderer.render(scene, camera)
  frameId = requestAnimationFrame(animate)
}

const handleClick = () => {
  if (!material) return

  assembled = !assembled
  clickTween?.kill()
  clickTween = gsap.to(progressState, {
    value: assembled ? 1 : 0,
    duration: 2.2,
    ease: 'power3.inOut',
    onUpdate: () => {
      uniforms.uProgress.value = progressState.value
    },
  })
}

const init = async () => {
  if (!canvasRef.value || !shouldRender.value || renderer) return

  await nextTick()

  renderer = new THREE.WebGLRenderer({
    canvas: canvasRef.value,
    alpha: true,
    antialias: true,
  })
  renderer.setClearColor(0x000000, 0)

  scene = new THREE.Scene()
  camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 100)
  camera.position.set(0, 0, 7)

  const imagePixels = await loadImagePixels(props.imageUrl)
  if (!renderer) return

  geometry = createParticleGeometry(imagePixels)
  material = new THREE.ShaderMaterial({
    uniforms,
    vertexShader,
    fragmentShader,
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
  })

  particles = new THREE.Points(geometry, material)
  scene.add(particles)

  resize()
  window.addEventListener('resize', resize)
  window.addEventListener('click', handleClick)
  animate()
}

const dispose = () => {
  if (frameId) cancelAnimationFrame(frameId)
  frameId = null

  window.removeEventListener('resize', resize)
  window.removeEventListener('click', handleClick)

  clickTween?.kill()
  clickTween = null

  if (particles) scene?.remove(particles)
  geometry?.dispose()
  material?.dispose()
  renderer?.dispose()

  renderer = null
  scene = null
  camera = null
  particles = null
  geometry = null
  material = null
  assembled = false
  progressState.value = 0
  uniforms.uProgress.value = 0
}

onMounted(() => {
  init()
})

onBeforeUnmount(dispose)

watch(shouldRender, (enabled) => {
  if (enabled) init()
  else dispose()
})
</script>

<template>
  <canvas
    v-if="shouldRender"
    ref="canvasRef"
    class="particle-photo-overlay"
    :style="{ opacity }"
  />
</template>
