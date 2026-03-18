// Animation System - handles blinking, breathing, and micro-movements
class CharacterAnimator {
    constructor(renderer, updateCallback) {
        this.renderer = renderer;
        this.updateCallback = updateCallback;

        // Animation state
        this.state = {
            blinking: false,
            mouthFrame: 0,
            breathingOffset: 0,
            hairPhase: 0 // For gentle hair sway animation
        };

        // Timing variables
        this.blinkTimer = 0;
        this.blinkDuration = 0;
        this.nextBlinkTime = this.randomBlinkInterval();

        this.mouthTimer = 0;
        this.mouthInterval = 3000; // Change mouth position every 3 seconds

        this.breathingTimer = 0;
        this.breathingInterval = 2000; // Breathing cycle

        this.hairTimer = 0;
        this.hairInterval = 100; // Hair sway update every 100ms for smooth animation

        this.lastTime = Date.now();
        this.isRunning = false;
        this.animationFrameId = null;
    }

    // Random blink interval (humans blink every 2-10 seconds)
    randomBlinkInterval() {
        return 2000 + Math.random() * 8000;
    }

    // Start animation loop
    start() {
        this.isRunning = true;
        this.lastTime = Date.now();
        this.animate();
    }

    // Stop animation loop
    stop() {
        this.isRunning = false;
        if (this.animationFrameId) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
        }
    }

    // Main animation loop
    animate() {
        if (!this.isRunning) return;

        const currentTime = Date.now();
        const deltaTime = currentTime - this.lastTime;
        this.lastTime = currentTime;

        let needsUpdate = false;

        // Handle blinking
        if (this.state.blinking) {
            this.blinkDuration -= deltaTime;
            if (this.blinkDuration <= 0) {
                this.state.blinking = false;
                needsUpdate = true;
            }
        } else {
            this.blinkTimer += deltaTime;
            if (this.blinkTimer >= this.nextBlinkTime) {
                // Start a blink
                this.state.blinking = true;
                this.blinkDuration = 100 + Math.random() * 100; // Blink lasts 100-200ms
                this.blinkTimer = 0;
                this.nextBlinkTime = this.randomBlinkInterval();
                needsUpdate = true;
            }
        }

        // Handle mouth micro-movements
        this.mouthTimer += deltaTime;
        if (this.mouthTimer >= this.mouthInterval) {
            this.mouthTimer = 0;
            this.state.mouthFrame = (this.state.mouthFrame + 1) % 3;
            this.mouthInterval = 2000 + Math.random() * 4000; // Random interval
            needsUpdate = true;
        }

        // Handle breathing (subtle movement) - not used in current render but available
        this.breathingTimer += deltaTime;
        if (this.breathingTimer >= this.breathingInterval) {
            this.breathingTimer = 0;
            this.state.breathingOffset = (this.state.breathingOffset + 1) % 2;
        }

        // Handle hair sway animation
        this.hairTimer += deltaTime;
        if (this.hairTimer >= this.hairInterval) {
            this.hairTimer = 0;
            this.state.hairPhase = (this.state.hairPhase + 1) % 360; // Full cycle
            needsUpdate = true; // Always update for smooth hair animation
        }

        // Update display if needed
        if (needsUpdate && this.updateCallback) {
            this.updateCallback(this.state);
        }

        // Continue animation loop
        this.animationFrameId = requestAnimationFrame(() => this.animate());
    }

    // Reset animation state
    reset() {
        this.state = {
            blinking: false,
            mouthFrame: 0,
            breathingOffset: 0,
            hairPhase: 0
        };
        this.blinkTimer = 0;
        this.nextBlinkTime = this.randomBlinkInterval();
        this.mouthTimer = 0;
        this.hairTimer = 0;
    }

    // Get current animation state
    getState() {
        return { ...this.state };
    }
}
