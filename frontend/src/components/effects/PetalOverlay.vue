<script setup>
import { ref, onMounted, onBeforeUnmount, watch, computed } from 'vue'
import { useEffects } from '../../composables/useEffects.js'

const props = defineProps({
  type: { type: String, default: 'none' },
  density: { type: Number, default: 15 },
  speed: { type: Number, default: 1 },
  opacity: { type: Number, default: 0.2 },
})

const { canAnimate, effectDensity } = useEffects()
const canvasRef = ref(null)
let animId = null
let particles = []

const actualDensity = computed(() => Math.round(props.density * effectDensity.value))

const colors = {
  petal: ['#ffb7c5', '#ffc0cb', '#ff9eb5', '#f8c8dc', '#e8a0bf'],
  sparkle: ['#fffbe6', '#fff5cc', '#ffeebb', '#e6f0ff', '#d4e8ff'],
  snow: ['#ffffff', '#f0f4ff', '#e8ecf4', '#dde4ee', '#f5f7fa'],
}

const createParticle = (canvas, type) => {
  const w = canvas.width
  const h = canvas.height
  return {
    x: Math.random() * w,
    y: Math.random() * h - h,
    size: type === 'snow' ? 2 + Math.random() * 4 : type === 'petal' ? 4 + Math.random() * 8 : 1.5 + Math.random() * 3,
    speedY: (0.3 + Math.random() * 0.7) * props.speed,
    speedX: (Math.random() - 0.5) * 0.5 * props.speed,
    rotation: Math.random() * Math.PI * 2,
    rotSpeed: (Math.random() - 0.5) * 0.02,
    color: colors[type]?.[Math.floor(Math.random() * (colors[type]?.length || 1))] || '#fff',
    alpha: 0.4 + Math.random() * 0.6,
  }
}

const drawPetal = (ctx, p) => {
  ctx.save()
  ctx.translate(p.x, p.y)
  ctx.rotate(p.rotation)
  ctx.fillStyle = p.color
  ctx.globalAlpha = p.alpha
  ctx.beginPath()
  ctx.ellipse(0, 0, p.size, p.size * 0.5, 0, 0, Math.PI * 2)
  ctx.fill()
  ctx.restore()
}

const drawSparkle = (ctx, p) => {
  ctx.save()
  ctx.translate(p.x, p.y)
  ctx.fillStyle = p.color
  ctx.globalAlpha = p.alpha * (0.5 + 0.5 * Math.sin(Date.now() * 0.003 + p.rotation))
  ctx.beginPath()
  ctx.arc(0, 0, p.size, 0, Math.PI * 2)
  ctx.fill()
  ctx.restore()
}

const drawSnow = (ctx, p) => {
  ctx.save()
  ctx.translate(p.x, p.y)
  ctx.fillStyle = p.color
  ctx.globalAlpha = p.alpha
  ctx.beginPath()
  ctx.arc(0, 0, p.size, 0, Math.PI * 2)
  ctx.fill()
  ctx.restore()
}

const drawFn = { petal: drawPetal, sparkle: drawSparkle, snow: drawSnow }

const animate = () => {
  const canvas = canvasRef.value
  if (!canvas) return
  const ctx = canvas.getContext('2d')
  const w = canvas.width
  const h = canvas.height

  ctx.clearRect(0, 0, w, h)
  const draw = drawFn[props.type]
  if (!draw) return

  for (const p of particles) {
    p.y += p.speedY
    p.x += p.speedX
    p.rotation += p.rotSpeed

    if (p.y > h + 20) {
      p.y = -20
      p.x = Math.random() * w
    }
    if (p.x < -20) p.x = w + 20
    if (p.x > w + 20) p.x = -20

    draw(ctx, p)
  }

  animId = requestAnimationFrame(animate)
}

const init = () => {
  const canvas = canvasRef.value
  if (!canvas || props.type === 'none' || !canAnimate.value) return

  const rect = canvas.parentElement.getBoundingClientRect()
  canvas.width = rect.width * (window.devicePixelRatio > 1 ? 1.5 : 1)
  canvas.height = rect.height * (window.devicePixelRatio > 1 ? 1.5 : 1)

  particles = []
  for (let i = 0; i < actualDensity.value; i++) {
    const p = createParticle(canvas, props.type)
    p.y = Math.random() * canvas.height // Spread initial positions
    particles.push(p)
  }

  animate()
}

const stop = () => {
  if (animId) cancelAnimationFrame(animId)
  animId = null
  particles = []
}

onMounted(() => {
  if (props.type !== 'none') init()
})

onBeforeUnmount(stop)

watch(() => props.type, (newType) => {
  stop()
  if (newType !== 'none') setTimeout(init, 50)
})

watch(canAnimate, (val) => {
  if (!val) stop()
  else if (props.type !== 'none') init()
})
</script>

<template>
  <canvas
    v-if="type !== 'none' && canAnimate"
    ref="canvasRef"
    class="petal-overlay"
    :style="{ opacity: opacity }"
  />
</template>
