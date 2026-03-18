// Main application logic - Robust version
class CharacterCreator {
    constructor() {
        this.renderer = new RobustFaceRenderer();
        this.animator = null;

        // Current character options
        this.options = {
            faceShape: 'oval',
            bodyType: 'average',
            eyeType: 'normal',
            noseType: 'medium',
            mouthType: 'normal',
            hairStyle: 'short',
            hairColor: 'brown',
            eyeColor: 'brown',
            skinTone: 'medium',
            facialHair: 'none',
            scars: 'none',
            tattoos: 'none',
            piercings: 'none',
            expression: 'none',
            glasses: 'none',
            earrings: 'none',
            hat: 'none',
            ageFeatures: 'none',
            freckles: 'none',
            accessories: true,
            eyePatch: false,
            faceMask: false,
            headband: false,
            necklace: false,
            choker: false,
            hairPart: 'none',
            bangs: 'none',
            windEffect: 'none',
            hairOptions: {},
            viewAngle: 'front',
            shading: 'none',
            lightDirection: 'front',
            shadingIntensity: 2
        };

        this.canvas = document.getElementById('characterCanvas');
        this.initializeControls();
        this.initializeAnimator();
        this.updateDisplay();
    }

    // Initialize all control event listeners
    initializeControls() {
        // Face shape
        document.getElementById('faceShape').addEventListener('change', (e) => {
            this.options.faceShape = e.target.value;
            this.updateDisplay();
        });

        // Body type
        document.getElementById('bodyType').addEventListener('change', (e) => {
            this.options.bodyType = e.target.value;
            this.updateDisplay();
        });

        // Eyes
        document.getElementById('eyeType').addEventListener('change', (e) => {
            this.options.eyeType = e.target.value;
            this.updateDisplay();
        });

        // Nose
        document.getElementById('noseType').addEventListener('change', (e) => {
            this.options.noseType = e.target.value;
            this.updateDisplay();
        });

        // Mouth
        document.getElementById('mouthType').addEventListener('change', (e) => {
            this.options.mouthType = e.target.value;
            this.updateDisplay();
        });

        // Hair
        document.getElementById('hairStyle').addEventListener('change', (e) => {
            this.options.hairStyle = e.target.value;
            this.updateDisplay();
        });

        // Hair color
        document.getElementById('hairColor').addEventListener('change', (e) => {
            this.options.hairColor = e.target.value;
            this.updateDisplay();
        });

        // Eye color
        document.getElementById('eyeColor').addEventListener('change', (e) => {
            this.options.eyeColor = e.target.value;
            this.updateDisplay();
        });

        // Skin tone
        document.getElementById('skinTone').addEventListener('change', (e) => {
            this.options.skinTone = e.target.value;
            this.updateDisplay();
        });

        // Facial hair
        document.getElementById('facialHair').addEventListener('change', (e) => {
            this.options.facialHair = e.target.value;
            this.updateDisplay();
        });

        // Scars
        document.getElementById('scars').addEventListener('change', (e) => {
            this.options.scars = e.target.value;
            this.updateDisplay();
        });

        // Tattoos
        document.getElementById('tattoos').addEventListener('change', (e) => {
            this.options.tattoos = e.target.value;
            this.updateDisplay();
        });

        // Piercings
        document.getElementById('piercings').addEventListener('change', (e) => {
            this.options.piercings = e.target.value;
            this.updateDisplay();
        });

        // Expression
        document.getElementById('expression').addEventListener('change', (e) => {
            this.options.expression = e.target.value;
            this.updateDisplay();
        });

        // Glasses
        document.getElementById('glasses').addEventListener('change', (e) => {
            this.options.glasses = e.target.value;
            this.updateDisplay();
        });

        // Earrings
        document.getElementById('earrings').addEventListener('change', (e) => {
            this.options.earrings = e.target.value;
            this.updateDisplay();
        });

        // Hat
        document.getElementById('hat').addEventListener('change', (e) => {
            this.options.hat = e.target.value;
            this.updateDisplay();
        });

        // Other Accessories
        document.getElementById('otherAccessory').addEventListener('change', (e) => {
            const value = e.target.value;
            // Reset all other accessories
            this.options.eyePatch = false;
            this.options.faceMask = false;
            this.options.headband = false;
            this.options.necklace = false;

            // Set the selected one
            if (value !== 'none') {
                this.options[value] = true;
            }
            this.updateDisplay();
        });

        // Age Features
        document.getElementById('ageFeatures').addEventListener('change', (e) => {
            this.options.ageFeatures = e.target.value;
            this.updateDisplay();
        });

        // Freckles
        document.getElementById('freckles').addEventListener('change', (e) => {
            this.options.freckles = e.target.value;
            this.updateDisplay();
        });

        // Hair Parting
        document.getElementById('hairPart').addEventListener('change', (e) => {
            this.options.hairPart = e.target.value;
            this.updateHairOptions();
            this.updateDisplay();
        });

        // Bangs
        document.getElementById('bangs').addEventListener('change', (e) => {
            this.options.bangs = e.target.value;
            this.updateHairOptions();
            this.updateDisplay();
        });

        // Wind Effect
        document.getElementById('windEffect').addEventListener('change', (e) => {
            this.options.windEffect = e.target.value;
            this.updateHairOptions();
            this.updateDisplay();
        });

        // View Angle
        document.getElementById('viewAngle').addEventListener('change', (e) => {
            this.options.viewAngle = e.target.value;
            this.updateDisplay();
        });

        // Shading
        document.getElementById('shading').addEventListener('change', (e) => {
            this.options.shading = e.target.value;
            // Set intensity based on shading level
            this.options.shadingIntensity = e.target.value === 'light' ? 1 : e.target.value === 'medium' ? 2 : 3;
            this.updateDisplay();
        });

        // Light Direction
        document.getElementById('lightDirection').addEventListener('change', (e) => {
            this.options.lightDirection = e.target.value;
            this.updateDisplay();
        });

        // Randomize button
        document.getElementById('randomize').addEventListener('click', () => {
            this.randomize();
        });

        // Export button
        document.getElementById('export').addEventListener('click', () => {
            this.exportCharacter();
        });
    }

    // Initialize animator
    initializeAnimator() {
        this.animator = new CharacterAnimator(this.renderer, (animState) => {
            this.updateDisplay(animState);
        });
        this.animator.start();
    }

    // Update hair options based on selected styling
    updateHairOptions() {
        this.options.hairOptions = {
            parting: this.options.hairPart,
            bangs: this.options.bangs,
            wind: this.options.windEffect
        };
    }

    // Update the character display
    updateDisplay(animState = null) {
        if (!animState) {
            animState = this.animator ? this.animator.getState() : {};
        }

        const rendered = this.renderer.renderWithColors(this.options, animState);
        this.canvas.innerHTML = rendered;

        // Update UI theme based on character colors
        this.updateTheme();
    }

    // Update UI color scheme based on character's hair color
    updateTheme() {
        const colorSystem = this.renderer.colorSystem;
        const hairColor = this.options.hairColor || 'brown';

        // Get the hair color ramp
        const hairRamp = colorSystem.getColorRamp('hair', hairColor);
        if (!hairRamp) return;

        const root = document.documentElement;

        // Set CSS variables based on hair color
        root.style.setProperty('--accent-primary', hairRamp.base);
        root.style.setProperty('--accent-bright', hairRamp.highlight);
        root.style.setProperty('--accent-dim', hairRamp.shadow);

        // Create glow colors with transparency
        const glowColor = this.hexToRgba(hairRamp.base, 0.4);
        const glowSoft = this.hexToRgba(hairRamp.shadow, 0.25);

        root.style.setProperty('--glow-primary', glowColor);
        root.style.setProperty('--glow-soft', glowSoft);
        root.style.setProperty('--border-glow', this.hexToRgba(hairRamp.base, 0.3));
    }

    // Convert hex color to rgba
    hexToRgba(hex, alpha) {
        const r = parseInt(hex.slice(1, 3), 16);
        const g = parseInt(hex.slice(3, 5), 16);
        const b = parseInt(hex.slice(5, 7), 16);
        return `rgba(${r}, ${g}, ${b}, ${alpha})`;
    }

    // Randomize all character options
    randomize() {
        const faceShapes = ['oval', 'round', 'square', 'angular', 'long', 'heart', 'diamond', 'triangular', 'oblong'];
        const bodyTypes = ['thin', 'average', 'athletic', 'heavy', 'corpulent'];
        const eyeTypes = ['normal', 'wide', 'narrow', 'almond', 'hooded', 'round', 'tired', 'deepset', 'protruding', 'monolid', 'upturned', 'downturned'];
        const noseTypes = ['small', 'medium', 'large', 'hooked', 'button', 'flat', 'pointed'];
        const mouthTypes = ['small', 'normal', 'wide', 'thin', 'full', 'smirk'];
        const hairStyles = ['bald', 'short', 'medium', 'long', 'curly', 'curly-short', 'curly-long', 'wavy', 'wavy-long', 'braided', 'dreadlocks', 'spiky', 'ponytail', 'bun', 'mohawk', 'shaved', 'pixie', 'bob'];
        const hairColors = ['black', 'darkBrown', 'brown', 'lightBrown', 'blonde', 'platinum', 'red', 'auburn', 'ginger', 'gray', 'white', 'blue', 'purple', 'pink', 'green'];
        const eyeColors = ['brown', 'darkBrown', 'hazel', 'green', 'blue', 'gray', 'amber', 'violet', 'red', 'gold'];
        const skinTones = ['pale', 'fair', 'light', 'medium', 'tan', 'olive', 'brown', 'darkBrown', 'deep'];
        const facialHairs = ['none', 'stubble', 'goatee', 'full', 'mustache', 'soul', 'mutton'];
        const scarsOptions = ['none', 'eyebrow', 'cheek', 'lip', 'nose', 'multiple'];
        const tattooOptions = ['none', 'tribal', 'teardrops', 'forehead', 'cheek'];
        const piercingOptions = ['none', 'nose', 'eyebrow', 'lip', 'multiple'];
        // Weight 'none' heavily so individual eye/mouth features are used more often
        // 60% chance of 'none' (individual features), 40% chance of expression
        const expressions = ['none', 'none', 'none', 'none', 'none', 'none', 'joy', 'sadness', 'anger', 'surprise'];
        const glassesOptions = ['none', 'glassesRound', 'glassesSquare', 'sunglasses', 'monocle', 'readingGlasses', 'aviator'];
        const earringsOptions = ['none', 'earringStuds', 'earringHoops', 'earringDangly'];
        const hatOptions = ['none', 'topHat', 'cap', 'beanie', 'fedora', 'bandana'];
        const ageFeaturesOptions = ['none', 'young', 'middleAged', 'elderly', 'frecklesOnly'];
        const frecklesOptions = ['none', 'light', 'medium', 'heavy'];
        const otherAccessoryOptions = ['none', 'headband', 'eyePatch', 'faceMask', 'necklace', 'choker'];
        const hairPartOptions = ['none', 'centerPart', 'sidePart'];
        const bangsOptions = ['none', 'straightBangs', 'sweptBangs', 'curtainBangs'];
        const windEffectOptions = ['none', 'windLeft', 'windRight'];
        const viewAngleOptions = ['front', 'threeFourthLeft', 'threeFourthRight'];
        const shadingOptions = ['none', 'light', 'medium', 'heavy'];
        const lightDirectionOptions = ['front', 'topLeft', 'topRight', 'top'];

        const getRandom = (arr) => arr[Math.floor(Math.random() * arr.length)];

        this.options.faceShape = getRandom(faceShapes);
        this.options.bodyType = getRandom(bodyTypes);
        this.options.eyeType = getRandom(eyeTypes);
        this.options.noseType = getRandom(noseTypes);
        this.options.mouthType = getRandom(mouthTypes);
        this.options.hairStyle = getRandom(hairStyles);
        this.options.hairColor = getRandom(hairColors);
        this.options.eyeColor = getRandom(eyeColors);
        this.options.skinTone = getRandom(skinTones);
        this.options.facialHair = getRandom(facialHairs);
        this.options.scars = getRandom(scarsOptions);
        this.options.tattoos = getRandom(tattooOptions);
        this.options.piercings = getRandom(piercingOptions);
        this.options.expression = getRandom(expressions);
        this.options.glasses = getRandom(glassesOptions);
        this.options.earrings = getRandom(earringsOptions);
        this.options.hat = getRandom(hatOptions);
        this.options.ageFeatures = getRandom(ageFeaturesOptions);
        this.options.freckles = getRandom(frecklesOptions);

        // Other accessory
        const otherAccessory = getRandom(otherAccessoryOptions);
        this.options.eyePatch = false;
        this.options.faceMask = false;
        this.options.headband = false;
        this.options.necklace = false;
        this.options.choker = false;
        if (otherAccessory !== 'none') {
            this.options[otherAccessory] = true;
        }

        // Hair styling options
        this.options.hairPart = getRandom(hairPartOptions);
        this.options.bangs = getRandom(bangsOptions);
        this.options.windEffect = getRandom(windEffectOptions);
        this.updateHairOptions();

        // View and shading options
        this.options.viewAngle = getRandom(viewAngleOptions);
        this.options.shading = getRandom(shadingOptions);
        this.options.lightDirection = getRandom(lightDirectionOptions);
        this.options.shadingIntensity = this.options.shading === 'light' ? 1 : this.options.shading === 'medium' ? 2 : 3;

        // Update all select elements
        document.getElementById('faceShape').value = this.options.faceShape;
        document.getElementById('bodyType').value = this.options.bodyType;
        document.getElementById('eyeType').value = this.options.eyeType;
        document.getElementById('noseType').value = this.options.noseType;
        document.getElementById('mouthType').value = this.options.mouthType;
        document.getElementById('hairStyle').value = this.options.hairStyle;
        document.getElementById('hairColor').value = this.options.hairColor;
        document.getElementById('eyeColor').value = this.options.eyeColor;
        document.getElementById('skinTone').value = this.options.skinTone;
        document.getElementById('facialHair').value = this.options.facialHair;
        document.getElementById('scars').value = this.options.scars;
        document.getElementById('tattoos').value = this.options.tattoos;
        document.getElementById('piercings').value = this.options.piercings;
        document.getElementById('expression').value = this.options.expression;
        document.getElementById('glasses').value = this.options.glasses;
        document.getElementById('earrings').value = this.options.earrings;
        document.getElementById('hat').value = this.options.hat;
        document.getElementById('ageFeatures').value = this.options.ageFeatures;
        document.getElementById('freckles').value = this.options.freckles;
        document.getElementById('otherAccessory').value = otherAccessory;
        document.getElementById('hairPart').value = this.options.hairPart;
        document.getElementById('bangs').value = this.options.bangs;
        document.getElementById('windEffect').value = this.options.windEffect;
        document.getElementById('viewAngle').value = this.options.viewAngle;
        document.getElementById('shading').value = this.options.shading;
        document.getElementById('lightDirection').value = this.options.lightDirection;

        this.updateDisplay();

        // Reset animation
        if (this.animator) {
            this.animator.reset();
        }
    }

    // Export character to console and clipboard
    exportCharacter() {
        const exportData = this.renderer.export(this.options);

        // Create a modal-like overlay
        const overlay = document.createElement('div');
        overlay.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.9);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 1000;
        `;

        const exportBox = document.createElement('div');
        exportBox.style.cssText = `
            background: #000;
            color: #0f0;
            border: 2px solid #0f0;
            padding: 20px;
            max-width: 800px;
            max-height: 80%;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            box-shadow: 0 0 30px rgba(0, 255, 0, 0.5);
        `;

        const pre = document.createElement('pre');
        pre.textContent = exportData;
        pre.style.cssText = `
            margin: 0;
            white-space: pre;
            font-size: 10px;
            line-height: 1;
        `;

        const closeBtn = document.createElement('button');
        closeBtn.textContent = '[ CLOSE ]';
        closeBtn.className = 'terminal-button';
        closeBtn.style.cssText = `
            margin-top: 20px;
            width: 100%;
        `;
        closeBtn.onclick = () => document.body.removeChild(overlay);

        const copyBtn = document.createElement('button');
        copyBtn.textContent = '[ COPY TO CLIPBOARD ]';
        copyBtn.className = 'terminal-button';
        copyBtn.style.cssText = `
            margin-top: 10px;
            width: 100%;
        `;
        copyBtn.onclick = () => {
            navigator.clipboard.writeText(exportData).then(() => {
                copyBtn.textContent = '[ COPIED! ]';
                setTimeout(() => {
                    copyBtn.textContent = '[ COPY TO CLIPBOARD ]';
                }, 2000);
            });
        };

        exportBox.appendChild(pre);
        exportBox.appendChild(copyBtn);
        exportBox.appendChild(closeBtn);
        overlay.appendChild(exportBox);
        document.body.appendChild(overlay);

        // Also log to console
        console.log(exportData);
    }
}

// Initialize application when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    const app = new CharacterCreator();

    // Add some helpful console messages
    console.log('%c╔════════════════════════════════════════╗', 'color: #0f0; font-family: monospace;');
    console.log('%c║   MUD Character Creator v2.0 ROBUST   ║', 'color: #0f0; font-family: monospace;');
    console.log('%c╚════════════════════════════════════════╝', 'color: #0f0; font-family: monospace;');
    console.log('%cRobust grid-based system initialized!', 'color: #0f0; font-family: monospace;');
    console.log('%cTip: Use RANDOMIZE to test different combinations', 'color: #0a0; font-family: monospace;');
});
