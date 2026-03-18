/**
 * Robust Face Renderer
 * Uses grid-based system with proper layering
 */

class RobustFaceRenderer {
    constructor() {
        this.gridSystem = new FaceGridSystem();
        this.colorSystem = new ColorSystem();
    }

    /**
     * Render complete face with all features
     */
    render(options, animState = {}) {
        const grid = this.gridSystem.createGrid();

        // Layer 1: Face shape (base)
        const faceShapeFunc = FaceFeatures.faceShapes[options.faceShape];
        if (faceShapeFunc) {
            faceShapeFunc(grid, this.gridSystem, {
                bodyType: this.getBodyTypeMod(options.bodyType)
            });
        }

        // Layer 2: Skin tone/shading
        this.addFacialShading(grid, options);

        // Layer 2.5: Apply depth shading and 3/4 view adjustments
        this.applyDepthShading(grid, options);
        this.applyViewAngle(grid, options);

        // Layer 3: Aging features (rendered early for depth)
        this.renderAgingFeatures(grid, options);

        // Layer 4: Facial features (eyes, nose, mouth)
        // ALWAYS render nose from dropdown selection
        const noseFunc = FaceFeatures.noses[options.noseType];
        if (noseFunc) {
            noseFunc(grid, this.gridSystem);
        }

        // Check if using combined expression for eyes/mouth
        if (options.expression && options.expression !== 'none') {
            this.renderExpression(grid, options.expression, animState);
        } else {
            // Render individual eyes and mouth from dropdowns
            const eyeFunc = FaceFeatures.eyes[options.eyeType];
            if (eyeFunc) {
                eyeFunc(grid, this.gridSystem, animState.blinking);
            }

            const mouthFunc = FaceFeatures.mouths[options.mouthType];
            if (mouthFunc) {
                const frame = animState.mouthFrame || 0;
                mouthFunc(grid, this.gridSystem, frame);
            }
        }

        // Layer 5: Hair (rendered before accessories for proper depth)
        this.renderHair(grid, options.hairStyle, options.hairOptions, animState);

        // Layer 6: Facial hair
        this.renderFacialHair(grid, options.facialHair);

        // Layer 7: Accessories (scars, tattoos, piercings, glasses, hats, etc.)
        this.renderScars(grid, options.scars);
        this.renderTattoos(grid, options.tattoos);
        this.renderPiercings(grid, options.piercings);
        this.renderAccessories(grid, options);

        // Convert to string
        return this.gridSystem.gridToString(grid);
    }

    /**
     * Render combined facial expression
     */
    renderExpression(grid, expression, animState) {
        const expressions = {
            joy: {
                eyes: 'happy',
                mouth: 'laughing'
            },
            sadness: {
                eyes: 'sad',
                mouth: 'frowning'
            },
            anger: {
                eyes: 'angry',
                mouth: 'frowning'
            },
            fear: {
                eyes: 'surprised',
                mouth: 'talking'  // Open mouth for fear
            },
            disgust: {
                eyes: 'skeptical',
                mouth: 'frowning'
            },
            surprise: {
                eyes: 'surprised',
                mouth: 'talking'  // Open mouth for surprise
            },
            contemplative: {
                eyes: 'skeptical',
                mouth: 'thin'
            },
            mischievous: {
                eyes: 'skeptical',
                mouth: 'smirk'
            },
            sleepy: {
                eyes: FaceFeatures.eyes.tired,
                mouth: 'small'
            }
        };

        const expr = expressions[expression];
        if (expr) {
            // Render eyes - use expressive eyes for the emotion
            if (typeof expr.eyes === 'string' && FaceFeatures.expressiveEyes[expr.eyes]) {
                FaceFeatures.expressiveEyes[expr.eyes](grid, this.gridSystem, animState.blinking);
            } else if (typeof expr.eyes === 'function') {
                expr.eyes(grid, this.gridSystem, animState.blinking);
            }

            // Render mouth - use expressive mouth for the emotion
            if (FaceFeatures.mouths[expr.mouth]) {
                FaceFeatures.mouths[expr.mouth](grid, this.gridSystem, animState.mouthFrame || 0);
            }
        }
    }

    /**
     * Render individual facial features (eyes, nose, mouth) from dropdown selections
     */
    renderIndividualFeatures(grid, options, animState) {
        const eyeFunc = FaceFeatures.eyes[options.eyeType];
        if (eyeFunc) {
            eyeFunc(grid, this.gridSystem, animState.blinking);
        }

        const noseFunc = FaceFeatures.noses[options.noseType];
        if (noseFunc) {
            noseFunc(grid, this.gridSystem);
        }

        const mouthFunc = FaceFeatures.mouths[options.mouthType];
        if (mouthFunc) {
            const frame = animState.mouthFrame || 0;
            mouthFunc(grid, this.gridSystem, frame);
        }
    }

    /**
     * Render aging features
     */
    renderAgingFeatures(grid, options) {
        if (!options.ageFeatures || options.ageFeatures === 'none') return;

        const { foreheadLines, crowsFeet, laughLines, ageSpots, freckles, saggingFeatures } = FaceFeatures.agingFeatures;

        switch (options.ageFeatures) {
            case 'young':
                // Just freckles for young characters
                if (options.freckles && options.freckles !== 'none') {
                    freckles(grid, this.gridSystem, options.freckles);
                }
                break;

            case 'middleAged':
                if (options.freckles && options.freckles !== 'none') {
                    freckles(grid, this.gridSystem, options.freckles);
                }
                foreheadLines(grid, this.gridSystem, 1);
                laughLines(grid, this.gridSystem);
                break;

            case 'elderly':
                if (options.freckles && options.freckles !== 'none') {
                    freckles(grid, this.gridSystem, options.freckles);
                }
                foreheadLines(grid, this.gridSystem, 3);
                crowsFeet(grid, this.gridSystem);
                laughLines(grid, this.gridSystem);
                ageSpots(grid, this.gridSystem, 4);
                saggingFeatures(grid, this.gridSystem);
                break;

            case 'frecklesOnly':
                freckles(grid, this.gridSystem, options.freckles || 'light');
                break;
        }
    }

    /**
     * Render accessories
     */
    renderAccessories(grid, options) {
        if (!options.accessories) return;

        const { accessories } = FaceFeatures;

        // Glasses
        if (options.glasses && options.glasses !== 'none' && accessories[options.glasses]) {
            accessories[options.glasses](grid, this.gridSystem);
        }

        // Earrings
        if (options.earrings && options.earrings !== 'none' && accessories[options.earrings]) {
            accessories[options.earrings](grid, this.gridSystem);
        }

        // Hats
        if (options.hat && options.hat !== 'none' && accessories[options.hat]) {
            accessories[options.hat](grid, this.gridSystem);
        }

        // Other accessories
        if (options.eyePatch && accessories.eyePatch) {
            accessories.eyePatch(grid, this.gridSystem);
        }

        if (options.faceMask && accessories.faceMask) {
            accessories.faceMask(grid, this.gridSystem);
        }

        if (options.headband && accessories.headband) {
            accessories.headband(grid, this.gridSystem);
        }

        if (options.necklace && accessories.necklace) {
            accessories.necklace(grid, this.gridSystem);
        }

        if (options.choker && accessories.choker) {
            accessories.choker(grid, this.gridSystem);
        }
    }

    /**
     * Get body type modifier for face width
     */
    getBodyTypeMod(bodyType) {
        const mods = {
            thin: -1,
            average: 0,
            athletic: 1,
            heavy: 2,
            corpulent: 3
        };
        return mods[bodyType] || 0;
    }

    /**
     * Add subtle facial shading for depth
     */
    addFacialShading(grid, options) {
        const { CENTER, EYE_LINE, NOSE_BASE, CHIN, FACE_LEFT, FACE_RIGHT,
                HAIRLINE, CHEEKBONE, FOREHEAD, MOUTH_CENTER } = this.gridSystem.PROPORTIONS;

        const lightDir = options.lightDirection || 'front';
        const faceWidth = FACE_RIGHT - FACE_LEFT;
        const faceHeight = CHIN - HAIRLINE;

        // Fill face area with base shading
        for (let y = HAIRLINE + 1; y < CHIN; y++) {
            // Calculate face width at this row (narrower at top and bottom)
            let rowWidth = faceWidth;
            if (y < EYE_LINE) {
                // Forehead - slightly narrower
                rowWidth = faceWidth - 2;
            } else if (y > CHEEKBONE) {
                // Below cheekbones - narrows toward chin
                const chinProgress = (y - CHEEKBONE) / (CHIN - CHEEKBONE);
                rowWidth = faceWidth - Math.floor(chinProgress * 8);
            }

            const rowLeft = CENTER - Math.floor(rowWidth / 2);
            const rowRight = CENTER + Math.floor(rowWidth / 2);

            for (let x = rowLeft; x <= rowRight; x++) {
                // Skip if already has content
                if (grid[y] && grid[y][x] && grid[y][x] !== ' ') continue;

                // Calculate shading based on position
                const distFromCenter = Math.abs(x - CENTER);
                const normalizedX = distFromCenter / (rowWidth / 2);
                const normalizedY = (y - HAIRLINE) / faceHeight;

                // Edge darkening (face contour)
                let shadeLevel = 0.7; // Base brightness

                // Horizontal falloff (edges darker)
                if (normalizedX > 0.6) {
                    shadeLevel -= (normalizedX - 0.6) * 0.8;
                }

                // Vertical shading
                if (y < FOREHEAD) {
                    shadeLevel -= 0.1; // Forehead slightly darker at top
                }
                if (y > MOUTH_CENTER) {
                    shadeLevel -= (normalizedY - 0.7) * 0.3; // Chin shadow
                }

                // Light direction adjustments
                if (lightDir === 'topLeft') {
                    if (x < CENTER) shadeLevel += 0.15;
                    else shadeLevel -= 0.15;
                } else if (lightDir === 'topRight') {
                    if (x > CENTER) shadeLevel += 0.15;
                    else shadeLevel -= 0.15;
                } else if (lightDir === 'top') {
                    if (y < EYE_LINE) shadeLevel += 0.1;
                    else shadeLevel -= normalizedY * 0.2;
                }

                // Cheekbone highlight
                if (y >= EYE_LINE + 2 && y <= CHEEKBONE &&
                    (normalizedX > 0.4 && normalizedX < 0.7)) {
                    shadeLevel += 0.15;
                }

                // Clamp and get character
                shadeLevel = Math.max(0.1, Math.min(1, shadeLevel));
                const shadeChar = this.gridSystem.getShadeChar(shadeLevel, 'SKIN');

                if (grid[y]) {
                    grid[y][x] = shadeChar;
                }
            }
        }

        // Add nose bridge shadow
        for (let y = EYE_LINE + 2; y < NOSE_BASE - 2; y++) {
            if (lightDir === 'topLeft' || lightDir === 'top') {
                if (grid[y]) grid[y][CENTER + 1] = '░';
            } else if (lightDir === 'topRight') {
                if (grid[y]) grid[y][CENTER - 1] = '░';
            }
        }

        // Under-eye area (slightly darker)
        for (let x = CENTER - 8; x <= CENTER + 8; x++) {
            if (Math.abs(x - CENTER) > 3 && Math.abs(x - CENTER) < 7) {
                if (grid[EYE_LINE + 1]) {
                    const current = grid[EYE_LINE + 1][x];
                    if (current === ' ' || current === '.' || current === ':') {
                        grid[EYE_LINE + 1][x] = '░';
                    }
                }
            }
        }
    }

    /**
     * Apply depth shading based on lighting direction
     */
    applyDepthShading(grid, options) {
        if (!options.shading || options.shading === 'none') return;

        const { shading } = FaceFeatures;
        if (!shading) return;

        const lightDirection = options.lightDirection || 'front';
        const shadingIntensity = options.shadingIntensity || 2;

        // Apply face contour shading
        if (shading.faceContour) {
            shading.faceContour(grid, this.gridSystem, lightDirection, shadingIntensity);
        }

        // Apply nose shadow
        if (shading.noseShadow) {
            shading.noseShadow(grid, this.gridSystem, lightDirection);
        }

        // Apply cheekbone highlights
        if (shading.cheekboneHighlight) {
            shading.cheekboneHighlight(grid, this.gridSystem, lightDirection);
        }

        // Apply chin shadow
        if (shading.chinShadow && options.shading !== 'light') {
            shading.chinShadow(grid, this.gridSystem, shadingIntensity);
        }

        // Apply eye socket depth
        if (shading.eyeSocketDepth && options.shading === 'heavy') {
            shading.eyeSocketDepth(grid, this.gridSystem);
        }
    }

    /**
     * Apply 3/4 view angle adjustments
     */
    applyViewAngle(grid, options) {
        if (!options.viewAngle || options.viewAngle === 'front') return;

        const { threeFourthView } = FaceFeatures;
        if (!threeFourthView) return;

        if (options.viewAngle === 'threeFourthLeft' && threeFourthView.adjustForLeftView) {
            threeFourthView.adjustForLeftView(grid, this.gridSystem);
        } else if (options.viewAngle === 'threeFourthRight' && threeFourthView.adjustForRightView) {
            threeFourthView.adjustForRightView(grid, this.gridSystem);
        }
    }

    /**
     * Render hair with advanced styles and shading
     */
    renderHair(grid, hairStyle, hairOptions, animState = {}) {
        const { CENTER, TOP_OF_HEAD, HAIRLINE, EYE_LINE, NOSE_BASE, MOUTH_CENTER, CHIN, FACE_LEFT, FACE_RIGHT } = this.gridSystem.PROPORTIONS;

        switch (hairStyle) {
            case 'bald':
                // No hair, maybe add shine on head
                for (let x = CENTER - 3; x <= CENTER + 3; x++) {
                    this.gridSystem.setChar(grid, x, TOP_OF_HEAD + 2, '·');
                }
                break;

            case 'short':
                // Short hair with shading
                for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT + 1; x <= FACE_RIGHT - 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            // Add shading based on position
                            const distFromCenter = Math.abs(x - CENTER);
                            const char = distFromCenter > 8 ? '▓' : distFromCenter > 5 ? '▒' : '#';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }
                break;

            case 'medium':
                // Medium length - covers forehead and sides
                for (let y = TOP_OF_HEAD - 2; y <= HAIRLINE + 1; y++) {
                    for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            const distFromEdge = Math.min(x - FACE_LEFT, FACE_RIGHT - x);
                            const char = distFromEdge < 2 ? '▓' : '#';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }

                // Sideburns with shading
                for (let y = HAIRLINE; y <= EYE_LINE + 2; y++) {
                    this.gridSystem.setChar(grid, FACE_LEFT + 1, y, '▓');
                    this.gridSystem.setChar(grid, FACE_RIGHT - 1, y, '▓');
                }
                break;

            case 'long':
                // Long hair - top and flowing down sides
                for (let y = TOP_OF_HEAD - 3; y <= HAIRLINE + 2; y++) {
                    for (let x = FACE_LEFT - 1; x <= FACE_RIGHT + 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '#');
                        }
                    }
                }

                // Long sides with natural fall
                for (let y = HAIRLINE; y <= NOSE_BASE + 5; y++) {
                    const distFromTop = y - HAIRLINE;
                    const char = distFromTop > 8 ? '║' : '▓';
                    this.gridSystem.setChar(grid, FACE_LEFT, y, char);
                    this.gridSystem.setChar(grid, FACE_LEFT + 1, y, char);
                    this.gridSystem.setChar(grid, FACE_RIGHT - 1, y, char);
                    this.gridSystem.setChar(grid, FACE_RIGHT, y, char);
                }
                break;

            case 'curly':
                // Curly hair with @ and mixed symbols for texture
                for (let y = TOP_OF_HEAD - 2; y <= HAIRLINE + 1; y++) {
                    for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            const rand = Math.random();
                            const char = rand > 0.7 ? '@' : rand > 0.4 ? '&' : rand > 0.2 ? '∞' : '%';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }
                break;

            case 'curly-short':
                // Short curly/afro style with tight curls
                for (let y = TOP_OF_HEAD - 1; y <= HAIRLINE + 1; y++) {
                    for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            const rand = Math.random();
                            const char = rand > 0.6 ? '○' : rand > 0.3 ? '◎' : '◯';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }
                break;

            case 'curly-long':
                // Long curly hair cascading down
                for (let y = TOP_OF_HEAD - 3; y <= HAIRLINE + 2; y++) {
                    for (let x = FACE_LEFT - 1; x <= FACE_RIGHT + 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            const char = Math.random() > 0.5 ? '@' : '&';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }
                // Curly cascading sides
                for (let y = HAIRLINE; y <= MOUTH_CENTER + 3; y++) {
                    const chars = ['@', '&', '∞', '%', '§'];
                    const char = chars[Math.floor(Math.random() * chars.length)];
                    this.gridSystem.setChar(grid, FACE_LEFT - 1, y, char);
                    this.gridSystem.setChar(grid, FACE_LEFT, y, char);
                    this.gridSystem.setChar(grid, FACE_RIGHT, y, char);
                    this.gridSystem.setChar(grid, FACE_RIGHT + 1, y, char);
                }
                break;

            case 'wavy':
                // Wavy hair with flowing S-curves
                for (let y = TOP_OF_HEAD - 2; y <= HAIRLINE + 1; y++) {
                    for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            const wave = Math.sin((x + y) * 0.5);
                            const char = wave > 0.3 ? '~' : wave > -0.3 ? '≈' : '∼';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }
                break;

            case 'wavy-long':
                // Long wavy flowing hair
                for (let y = TOP_OF_HEAD - 3; y <= HAIRLINE + 1; y++) {
                    for (let x = FACE_LEFT - 1; x <= FACE_RIGHT + 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            const char = Math.random() > 0.5 ? '~' : '≈';
                            this.gridSystem.setChar(grid, x, y, char);
                        }
                    }
                }
                // Flowing wavy sides
                for (let y = HAIRLINE; y <= NOSE_BASE + 4; y++) {
                    const waveOffset = Math.sin(y * 0.5);
                    const leftX = FACE_LEFT + Math.round(waveOffset);
                    const rightX = FACE_RIGHT - Math.round(waveOffset);
                    this.gridSystem.setChar(grid, leftX, y, '~');
                    this.gridSystem.setChar(grid, leftX + 1, y, '≈');
                    this.gridSystem.setChar(grid, rightX - 1, y, '≈');
                    this.gridSystem.setChar(grid, rightX, y, '~');
                }
                break;

            case 'braided':
                // Braided hair on top
                for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT + 1; x <= FACE_RIGHT - 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '#');
                        }
                    }
                }
                // Braid pattern down the back
                for (let y = HAIRLINE; y <= NOSE_BASE + 8; y++) {
                    const pattern = y % 3;
                    if (pattern === 0) {
                        this.gridSystem.setChar(grid, FACE_RIGHT + 2, y, '╱');
                        this.gridSystem.setChar(grid, FACE_RIGHT + 3, y, '╲');
                    } else if (pattern === 1) {
                        this.gridSystem.setChar(grid, FACE_RIGHT + 2, y, '╲');
                        this.gridSystem.setChar(grid, FACE_RIGHT + 3, y, '╱');
                    } else {
                        this.gridSystem.setChar(grid, FACE_RIGHT + 2, y, '╳');
                    }
                }
                break;

            case 'dreadlocks':
                // Dreadlocks cascading down
                for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT + 1; x <= FACE_RIGHT - 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '▓');
                        }
                    }
                }
                // Individual dreads
                for (let x = FACE_LEFT; x <= FACE_RIGHT; x += 2) {
                    for (let y = HAIRLINE; y <= NOSE_BASE + 6; y++) {
                        this.gridSystem.setChar(grid, x, y, '║');
                    }
                }
                break;

            case 'spiky':
                // Spiky hair on top
                for (let x = FACE_LEFT + 2; x <= FACE_RIGHT - 2; x += 2) {
                    this.gridSystem.setChar(grid, x, TOP_OF_HEAD - 2, '|');
                    this.gridSystem.setChar(grid, x, TOP_OF_HEAD - 1, '|');
                    this.gridSystem.setChar(grid, x, TOP_OF_HEAD, '^');
                }

                for (let y = TOP_OF_HEAD + 1; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT + 1; x <= FACE_RIGHT - 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '#');
                        }
                    }
                }
                break;

            case 'ponytail':
                // Hair on top
                for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT + 1; x <= FACE_RIGHT - 1; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '#');
                        }
                    }
                }

                // Ponytail at back
                const ponytailX = FACE_RIGHT + 3;
                for (let y = HAIRLINE; y <= HAIRLINE + 8; y++) {
                    this.gridSystem.setChar(grid, ponytailX, y, '#');
                    this.gridSystem.setChar(grid, ponytailX + 1, y, '#');
                }
                break;

            case 'bun':
                // Hair pulled back into bun
                for (let y = TOP_OF_HEAD; y <= HAIRLINE - 1; y++) {
                    for (let x = FACE_LEFT + 2; x <= FACE_RIGHT - 2; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '·');
                        }
                    }
                }
                // Bun at top-back
                const bunX = FACE_RIGHT + 1;
                const bunY = TOP_OF_HEAD + 2;
                this.gridSystem.fillCircle(grid, bunX, bunY, 2, '●');
                break;

            case 'mohawk':
                // Central strip of hair
                for (let y = TOP_OF_HEAD - 3; y <= HAIRLINE; y++) {
                    for (let x = CENTER - 2; x <= CENTER + 2; x++) {
                        const height = TOP_OF_HEAD - y;
                        const char = height > 2 ? '▲' : '║';
                        this.gridSystem.setChar(grid, x, y, char);
                    }
                }
                break;

            case 'shaved':
                // Very short stubble with shading
                for (let y = TOP_OF_HEAD + 1; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT + 1; x <= FACE_RIGHT - 1; x += 2) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '░');
                        }
                    }
                }
                break;

            case 'pixie':
                // Short pixie cut
                for (let y = TOP_OF_HEAD; y <= HAIRLINE - 1; y++) {
                    for (let x = FACE_LEFT + 2; x <= FACE_RIGHT - 2; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '#');
                        }
                    }
                }
                // Short side wisps
                for (let y = HAIRLINE; y <= EYE_LINE; y++) {
                    this.gridSystem.setChar(grid, FACE_LEFT + 1, y, '/');
                    this.gridSystem.setChar(grid, FACE_RIGHT - 1, y, '\\');
                }
                break;

            case 'bob':
                // Bob cut with straight lines
                for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                    for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                        if (grid[y][x] === ' ' || grid[y][x] === '-') {
                            this.gridSystem.setChar(grid, x, y, '#');
                        }
                    }
                }
                // Straight bob sides
                for (let y = HAIRLINE; y <= NOSE_BASE; y++) {
                    this.gridSystem.setChar(grid, FACE_LEFT, y, '║');
                    this.gridSystem.setChar(grid, FACE_LEFT + 1, y, '#');
                    this.gridSystem.setChar(grid, FACE_RIGHT - 1, y, '#');
                    this.gridSystem.setChar(grid, FACE_RIGHT, y, '║');
                }
                break;
        }

        // Apply hair styling options after base hair is rendered
        if (hairOptions && FaceFeatures.hairOptions) {
            // Apply parting
            if (hairOptions.parting && hairOptions.parting !== 'none') {
                const partFunc = FaceFeatures.hairOptions[hairOptions.parting];
                if (partFunc) partFunc(grid, this.gridSystem);
            }

            // Apply bangs
            if (hairOptions.bangs && hairOptions.bangs !== 'none') {
                const bangsFunc = FaceFeatures.hairOptions[hairOptions.bangs];
                if (bangsFunc) bangsFunc(grid, this.gridSystem);
            }

            // Apply wind effect
            if (hairOptions.wind && hairOptions.wind !== 'none') {
                const windFunc = FaceFeatures.hairOptions[hairOptions.wind];
                if (windFunc) windFunc(grid, this.gridSystem, hairStyle, animState);
            }
        }
    }

    /**
     * Render facial hair
     */
    renderFacialHair(grid, facialHair) {
        const { CENTER, NOSE_BASE, MOUTH_CENTER, CHIN, FACE_LEFT, FACE_RIGHT } = this.gridSystem.PROPORTIONS;

        switch (facialHair) {
            case 'stubble':
                // Light stubble on lower face
                for (let y = NOSE_BASE + 1; y <= CHIN - 1; y++) {
                    for (let x = FACE_LEFT + 3; x <= FACE_RIGHT - 3; x += 2) {
                        if (grid[y][x] === ' ') {
                            this.gridSystem.setChar(grid, x, y, '·');
                        }
                    }
                }
                break;

            case 'goatee':
                // Goatee on chin
                for (let y = MOUTH_CENTER + 2; y <= CHIN - 2; y++) {
                    for (let x = CENTER - 2; x <= CENTER + 2; x++) {
                        if (grid[y][x] === ' ') {
                            this.gridSystem.setChar(grid, x, y, ':');
                        }
                    }
                }
                break;

            case 'full':
                // Full beard
                for (let y = NOSE_BASE + 1; y <= CHIN - 1; y++) {
                    for (let x = FACE_LEFT + 2; x <= FACE_RIGHT - 2; x++) {
                        if (grid[y][x] === ' ') {
                            this.gridSystem.setChar(grid, x, y, ':');
                        }
                    }
                }

                // Mustache
                for (let x = CENTER - 4; x <= CENTER + 4; x++) {
                    if (grid[MOUTH_CENTER - 1][x] === ' ') {
                        this.gridSystem.setChar(grid, x, MOUTH_CENTER - 1, ':');
                    }
                }
                break;

            case 'mustache':
                // Just mustache
                for (let x = CENTER - 4; x <= CENTER + 4; x++) {
                    this.gridSystem.setChar(grid, x, MOUTH_CENTER - 1, ':');
                    this.gridSystem.setChar(grid, x, MOUTH_CENTER - 2, ':');
                }
                break;

            case 'soul':
                // Soul patch
                for (let y = MOUTH_CENTER + 2; y <= MOUTH_CENTER + 4; y++) {
                    this.gridSystem.setChar(grid, CENTER, y, ':');
                }
                break;

            case 'mutton':
                // Mutton chops - sideburns
                const { EYE_LINE } = this.gridSystem.PROPORTIONS;
                for (let y = EYE_LINE; y <= MOUTH_CENTER; y++) {
                    this.gridSystem.setChar(grid, FACE_LEFT + 2, y, ':');
                    this.gridSystem.setChar(grid, FACE_LEFT + 3, y, ':');
                    this.gridSystem.setChar(grid, FACE_RIGHT - 2, y, ':');
                    this.gridSystem.setChar(grid, FACE_RIGHT - 3, y, ':');
                }
                break;
        }
    }

    /**
     * Render scars
     */
    renderScars(grid, scars) {
        const { CENTER, EYE_LINE, NOSE_BASE, MOUTH_CENTER, FACE_LEFT, FACE_RIGHT } = this.gridSystem.PROPORTIONS;

        switch (scars) {
            case 'eyebrow':
                this.gridSystem.drawLine(grid, CENTER - 4, EYE_LINE - 3, CENTER - 1, EYE_LINE - 2, '/');
                break;

            case 'cheek':
                this.gridSystem.drawLine(grid, FACE_LEFT + 4, NOSE_BASE, FACE_LEFT + 6, NOSE_BASE + 2, '/');
                break;

            case 'lip':
                this.gridSystem.setChar(grid, CENTER + 3, MOUTH_CENTER, '|');
                this.gridSystem.setChar(grid, CENTER + 3, MOUTH_CENTER + 1, '|');
                break;

            case 'nose':
                this.gridSystem.setChar(grid, CENTER, NOSE_BASE - 2, 'x');
                break;

            case 'multiple':
                this.gridSystem.drawLine(grid, CENTER - 4, EYE_LINE - 3, CENTER - 1, EYE_LINE - 2, '/');
                this.gridSystem.drawLine(grid, FACE_RIGHT - 5, NOSE_BASE + 1, FACE_RIGHT - 3, NOSE_BASE + 3, '/');
                this.gridSystem.setChar(grid, CENTER - 3, MOUTH_CENTER + 2, 'x');
                break;
        }
    }

    /**
     * Render tattoos
     */
    renderTattoos(grid, tattoos) {
        const { CENTER, HAIRLINE, EYE_LINE, NOSE_BASE, FACE_LEFT, FACE_RIGHT } = this.gridSystem.PROPORTIONS;

        switch (tattoos) {
            case 'tribal':
                // Forehead tribal
                this.gridSystem.setChar(grid, CENTER - 3, HAIRLINE + 1, '▓');
                this.gridSystem.setChar(grid, CENTER - 2, HAIRLINE + 2, '░');
                this.gridSystem.setChar(grid, CENTER + 2, HAIRLINE + 2, '░');
                this.gridSystem.setChar(grid, CENTER + 3, HAIRLINE + 1, '▓');
                break;

            case 'teardrops':
                this.gridSystem.setChar(grid, CENTER - 5, EYE_LINE + 1, '◆');
                this.gridSystem.setChar(grid, CENTER + 5, EYE_LINE + 1, '◆');
                break;

            case 'forehead':
                this.gridSystem.setChar(grid, CENTER, HAIRLINE + 2, '◊');
                break;

            case 'cheek':
                for (let i = 0; i < 3; i++) {
                    this.gridSystem.setChar(grid, FACE_LEFT + 4, NOSE_BASE - 1 + i, '━');
                    this.gridSystem.setChar(grid, FACE_RIGHT - 4, NOSE_BASE - 1 + i, '━');
                }
                break;
        }
    }

    /**
     * Render piercings
     */
    renderPiercings(grid, piercings) {
        const { CENTER, EYE_LINE, NOSE_BASE, MOUTH_CENTER } = this.gridSystem.PROPORTIONS;

        switch (piercings) {
            case 'nose':
                this.gridSystem.setChar(grid, CENTER + 1, NOSE_BASE, 'o');
                break;

            case 'eyebrow':
                this.gridSystem.setChar(grid, CENTER + 5, EYE_LINE - 2, '─');
                break;

            case 'lip':
                this.gridSystem.setChar(grid, CENTER, MOUTH_CENTER + 1, 'o');
                break;

            case 'multiple':
                this.gridSystem.setChar(grid, CENTER + 1, NOSE_BASE, 'o');
                this.gridSystem.setChar(grid, CENTER + 5, EYE_LINE - 2, '─');
                this.gridSystem.setChar(grid, CENTER, MOUTH_CENTER + 1, 'o');
                break;
        }
    }

    /**
     * Render with HTML colors
     */
    renderWithColors(options, animState = {}) {
        const grid = this.gridSystem.createGrid();

        // Layer 1: Face shape (base)
        const faceShapeFunc = FaceFeatures.faceShapes[options.faceShape];
        if (faceShapeFunc) {
            faceShapeFunc(grid, this.gridSystem, {
                bodyType: this.getBodyTypeMod(options.bodyType)
            });
        }

        // Layer 2: Skin tone/shading
        this.addFacialShading(grid, options);

        // Layer 2.5: Apply depth shading and 3/4 view adjustments
        this.applyDepthShading(grid, options);
        this.applyViewAngle(grid, options);

        // Layer 3: Aging features
        this.renderAgingFeatures(grid, options);

        // Layer 4: Facial features (eyes, nose, mouth)
        // ALWAYS render nose from dropdown selection
        const noseFunc = FaceFeatures.noses[options.noseType];
        if (noseFunc) {
            noseFunc(grid, this.gridSystem);
        }

        // Check if using combined expression for eyes/mouth
        if (options.expression && options.expression !== 'none') {
            this.renderExpression(grid, options.expression, animState);
        } else {
            // Render individual eyes and mouth from dropdowns
            const eyeFunc = FaceFeatures.eyes[options.eyeType];
            if (eyeFunc) {
                eyeFunc(grid, this.gridSystem, animState.blinking);
            }

            const mouthFunc = FaceFeatures.mouths[options.mouthType];
            if (mouthFunc) {
                const frame = animState.mouthFrame || 0;
                mouthFunc(grid, this.gridSystem, frame);
            }
        }

        // Layer 5: Hair
        this.renderHair(grid, options.hairStyle, options.hairOptions, animState);

        // Layer 6: Facial hair
        this.renderFacialHair(grid, options.facialHair);

        // Layer 7: Accessories
        this.renderScars(grid, options.scars);
        this.renderTattoos(grid, options.tattoos);
        this.renderPiercings(grid, options.piercings);
        this.renderAccessories(grid, options);

        // Apply colors to grid and convert to HTML
        return this.gridToColoredHTML(grid, options);
    }

    /**
     * Convert grid to colored HTML with dithering and multi-shade support
     */
    gridToColoredHTML(grid, options) {
        const { CENTER, EYE_LINE, NOSE_BASE, MOUTH_CENTER, CHIN, FACE_LEFT, FACE_RIGHT, TOP_OF_HEAD } = this.gridSystem.PROPORTIONS;

        const hairColorName = options.hairColor || 'brown';
        const eyeColorName = options.eyeColor || 'brown';
        const skinToneName = options.skinTone || 'medium';

        // Get color ramps
        const hairRamp = this.colorSystem.getColorRamp('hair', hairColorName);
        const eyeRamp = this.colorSystem.getColorRamp('eyes', eyeColorName);
        const skinRamp = this.colorSystem.getColorRamp('skin', skinToneName);

        // Light direction affects shading
        const lightDir = options.lightDirection || 'front';
        const lightOffsetX = lightDir === 'topLeft' ? -5 : lightDir === 'topRight' ? 5 : 0;
        const lightOffsetY = lightDir === 'top' ? -3 : (lightDir.includes('top') ? -2 : 0);

        let html = '';
        for (let y = 0; y < grid.length; y++) {
            for (let x = 0; x < grid[y].length; x++) {
                const char = grid[y][x];
                let color = null;
                let textShadow = '';

                if (char !== ' ') {
                    // Calculate position-based shading
                    const distFromCenter = Math.abs(x - CENTER);
                    const distFromTop = y - TOP_OF_HEAD;
                    const faceWidth = FACE_RIGHT - FACE_LEFT;

                    // Normalized position for shading (0 = center/top, 1 = edge/bottom)
                    const horizontalPos = distFromCenter / (faceWidth / 2);
                    const verticalPos = (y - TOP_OF_HEAD) / (CHIN - TOP_OF_HEAD);

                    // Light influence
                    const lightInfluence = ((x - CENTER - lightOffsetX) / faceWidth) + 0.5;

                    // Hair characters - with depth shading
                    if ('#@&∞%§○◎◯~≈∼╱╲╳║▓▒░^|▲·'.includes(char) && y < EYE_LINE + 5) {
                        const shade = this.getHairShade(x, y, CENTER, TOP_OF_HEAD, lightDir, hairRamp);
                        color = shade;
                        // Add subtle glow to hair highlights
                        if (shade === hairRamp?.highlight) {
                            textShadow = `0 0 2px ${shade}`;
                        }
                    }
                    // Eye characters - vivid with highlight
                    else if ('oO●◉◕◔◠◡'.includes(char)) {
                        // Determine if highlight or main iris
                        if (char === '◔' || char === '◕') {
                            color = eyeRamp?.highlight || eyeRamp?.base;
                            textShadow = `0 0 3px ${eyeRamp?.highlight}`;
                        } else {
                            color = eyeRamp?.iris || eyeRamp?.base;
                        }
                    }
                    // Pupil
                    else if (char === '•' || char === '∙') {
                        color = eyeRamp?.pupil || '#000';
                    }
                    // Facial hair - follows hair color with depth
                    else if (char === ':' || char === ';') {
                        const shade = horizontalPos > 0.6 ? 'shadow' : 'base';
                        color = hairRamp?.[shade] || hairRamp?.base;
                    }
                    // Dither characters for shading
                    else if ('░▒▓█'.includes(char)) {
                        // These are shading chars - use skin shadow tones
                        const ditherShade = char === '░' ? 'base' :
                                           char === '▒' ? 'shadow' :
                                           char === '▓' ? 'deep' : 'deep';
                        color = skinRamp?.[ditherShade] || skinRamp?.shadow;
                    }
                    // Blush/cheek area
                    else if (y >= EYE_LINE + 2 && y <= NOSE_BASE &&
                             ((x >= FACE_LEFT + 2 && x <= FACE_LEFT + 5) ||
                              (x >= FACE_RIGHT - 5 && x <= FACE_RIGHT - 2))) {
                        color = skinRamp?.blush || skinRamp?.base;
                    }
                    // Skin/face - position-based shading
                    else {
                        color = this.getSkinShade(x, y, CENTER, FACE_LEFT, FACE_RIGHT,
                                                  EYE_LINE, CHIN, lightDir, skinRamp);
                    }
                }

                if (color) {
                    const style = textShadow ?
                        `color:${color};text-shadow:${textShadow}` :
                        `color:${color}`;
                    html += `<span style="${style}">${char}</span>`;
                } else {
                    html += char;
                }
            }
            html += '\n';
        }
        return html;
    }

    /**
     * Calculate hair shade based on position and lighting
     */
    getHairShade(x, y, centerX, topY, lightDir, colorRamp) {
        if (!colorRamp) return '#888';

        const distFromCenter = Math.abs(x - centerX);
        const distFromTop = y - topY;

        // Top center = highlight, edges and bottom = shadow
        let shade = 'base';

        if (lightDir === 'topLeft') {
            if (x < centerX - 3 && distFromTop < 5) shade = 'highlight';
            else if (x > centerX + 5) shade = 'shadow';
            else if (distFromTop > 8) shade = 'shadow';
        } else if (lightDir === 'topRight') {
            if (x > centerX + 3 && distFromTop < 5) shade = 'highlight';
            else if (x < centerX - 5) shade = 'shadow';
            else if (distFromTop > 8) shade = 'shadow';
        } else {
            // Front/top lighting
            if (distFromCenter < 4 && distFromTop < 4) shade = 'highlight';
            else if (distFromCenter > 8 || distFromTop > 10) shade = 'shadow';
            else if (distFromCenter > 10) shade = 'deep';
        }

        return colorRamp[shade] || colorRamp.base;
    }

    /**
     * Calculate skin shade based on position and lighting
     */
    getSkinShade(x, y, centerX, faceLeft, faceRight, eyeLine, chin, lightDir, colorRamp) {
        if (!colorRamp) return '#c8a090';

        const distFromCenter = Math.abs(x - centerX);
        const faceWidth = faceRight - faceLeft;
        const normalizedX = (x - faceLeft) / faceWidth;
        const normalizedY = (y - eyeLine) / (chin - eyeLine);

        let shade = 'base';

        // Edge darkening (face contour)
        if (distFromCenter > faceWidth * 0.35) {
            shade = 'shadow';
        }
        if (distFromCenter > faceWidth * 0.42) {
            shade = 'deep';
        }

        // Lighting adjustments
        if (lightDir === 'topLeft') {
            if (normalizedX < 0.4 && normalizedY < 0.3) shade = 'highlight';
            else if (normalizedX > 0.7) shade = 'shadow';
        } else if (lightDir === 'topRight') {
            if (normalizedX > 0.6 && normalizedY < 0.3) shade = 'highlight';
            else if (normalizedX < 0.3) shade = 'shadow';
        } else if (lightDir === 'top') {
            if (normalizedY < 0.2 && distFromCenter < faceWidth * 0.25) shade = 'highlight';
            else if (normalizedY > 0.7) shade = 'shadow';
        } else {
            // Front lighting - central highlight
            if (distFromCenter < faceWidth * 0.15 && normalizedY > 0.1 && normalizedY < 0.5) {
                shade = 'highlight';
            }
        }

        // Chin shadow
        if (normalizedY > 0.85) {
            shade = shade === 'deep' ? 'deep' : 'shadow';
        }

        // Under-eye area slightly darker
        if (normalizedY < 0.15 && normalizedY > 0) {
            if (shade === 'base') shade = 'shadow';
        }

        return colorRamp[shade] || colorRamp.base;
    }

    /**
     * Export character
     */
    export(options) {
        const output = this.render(options);
        return `
╔════════════════════════════════════════╗
║       CHARACTER EXPORT DATA            ║
╚════════════════════════════════════════╝

${output}

Configuration:
─────────────────────────────────────────
Face Shape: ${options.faceShape}
Body Type: ${options.bodyType}
Eyes: ${options.eyeType} (${options.eyeColor || 'brown'})
Nose: ${options.noseType}
Mouth: ${options.mouthType}
Hair: ${options.hairStyle} (${options.hairColor || 'brown'})
Skin Tone: ${options.skinTone || 'medium'}
Facial Hair: ${options.facialHair}
Scars: ${options.scars}
Tattoos: ${options.tattoos}
Piercings: ${options.piercings}
─────────────────────────────────────────
Generated: ${new Date().toISOString()}
        `;
    }
}
