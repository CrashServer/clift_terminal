/**
 * Grid-Based ASCII Face Generation System
 * Based on classical portrait proportions and Paul Bourke's density ramps
 *
 * Grid: 40 columns × 60 rows (taller for realistic face proportions)
 *
 * Anatomical Proportions (from SLYNYRD/classical art):
 * - Eyes at midpoint between top of head and chin
 * - Face divided into thirds (hairline→brows, brows→nose, nose→chin)
 * - Nose bottom halfway between eyes and chin
 * - Mouth 1/3 between nose and chin
 * - Eye spacing = 1 eye width
 * - Mouth width = distance between pupils
 */

class FaceGridSystem {
    constructor() {
        // Grid dimensions - taller for realistic proportions
        this.GRID_WIDTH = 40;
        this.GRID_HEIGHT = 60;

        // Anatomically correct facial proportion markers
        // Based on: eyes at 1/2, nose at 3/4, mouth at 7/8
        this.PROPORTIONS = {
            // Vertical divisions (rows) - anatomically correct
            TOP_OF_HEAD: 4,
            HAIRLINE: 10,
            FOREHEAD: 16,
            EYEBROW_LINE: 24,
            EYE_LINE: 28,          // Halfway between top (4) and chin (52)
            CHEEKBONE: 34,
            NOSE_BASE: 40,         // Halfway between eyes (28) and chin (52)
            UPPER_LIP: 44,
            MOUTH_CENTER: 46,      // 1/2 between nose (40) and chin (52)
            LOWER_LIP: 48,
            CHIN: 52,
            NECK_START: 54,

            // Horizontal divisions (columns - center = 20)
            CENTER: 20,
            EYE_LEFT_CENTER: 13,   // ~7 chars from center
            EYE_RIGHT_CENTER: 27,  // ~7 chars from center
            FACE_LEFT: 7,          // Face width ~26 chars
            FACE_RIGHT: 33,
            NOSE_LEFT: 17,
            NOSE_RIGHT: 23,
            MOUTH_LEFT: 14,
            MOUTH_RIGHT: 26
        };

        // Character density ramps for shading (Paul Bourke standard)
        // Darkest to lightest
        this.DENSITY_RAMP = {
            // Full 10-level ramp
            FULL: ['@', '%', '#', '*', '+', '=', '-', ':', '.', ' '],
            // Block shading
            BLOCK: ['█', '▓', '▒', '░', ' '],
            // Hair textures (dark to light)
            HAIR: ['█', '▓', '#', '@', '%', '&', '*', '≈', '~', '.'],
            // Skin shading (subtle gradients)
            SKIN: ['▓', '▒', '░', ':', '.', ' '],
            // Fine detail
            FINE: ['●', '○', '◐', '◑', '◒', '◓', '°', '·', '.', ' ']
        };

        // Facial contour characters
        this.CONTOURS = {
            // Face outline
            LEFT_EDGE: ['(', 'C', '{', '[', '/', '\\'],
            RIGHT_EDGE: [')', 'D', '}', ']', '\\', '/'],
            // Curves
            TOP_CURVE: ['‾', '⌒', '︵', '~', '-'],
            BOTTOM_CURVE: ['_', '⌣', '︶', '~', '-'],
            // Structural
            VERTICAL: ['│', '|', '║', 'I', 'l'],
            HORIZONTAL: ['─', '-', '═', '=', '_']
        };
    }

    /**
     * Create empty grid
     */
    createGrid() {
        const grid = [];
        for (let y = 0; y < this.GRID_HEIGHT; y++) {
            grid[y] = [];
            for (let x = 0; x < this.GRID_WIDTH; x++) {
                grid[y][x] = ' ';
            }
        }
        return grid;
    }

    /**
     * Get character for shading based on intensity (0-1)
     * 0 = darkest, 1 = lightest
     */
    getShadeChar(intensity, rampType = 'FULL') {
        const ramp = this.DENSITY_RAMP[rampType] || this.DENSITY_RAMP.FULL;
        const index = Math.floor(intensity * (ramp.length - 1));
        return ramp[Math.min(index, ramp.length - 1)];
    }

    /**
     * Calculate shading intensity based on position and light source
     * Returns 0-1 (dark to light)
     */
    calculateShading(x, y, lightX, lightY, centerX, centerY, radius) {
        // Distance from light source
        const dx = x - lightX;
        const dy = y - lightY;
        const distFromLight = Math.sqrt(dx * dx + dy * dy);

        // Distance from face center (for falloff)
        const dcx = x - centerX;
        const dcy = y - centerY;
        const distFromCenter = Math.sqrt(dcx * dcx + dcy * dcy);

        // Normalize and combine
        const maxDist = radius * 2;
        const lightIntensity = 1 - Math.min(distFromLight / maxDist, 1);
        const edgeFalloff = distFromCenter / radius;

        return Math.max(0, Math.min(1, lightIntensity * (1 - edgeFalloff * 0.5)));
    }

    /**
     * Set character at position
     */
    setChar(grid, x, y, char, layer = null) {
        x = Math.round(x);
        y = Math.round(y);
        if (y >= 0 && y < this.GRID_HEIGHT && x >= 0 && x < this.GRID_WIDTH) {
            if (layer) {
                grid[y][x] = { char, layer };
            } else {
                grid[y][x] = char;
            }
        }
    }

    /**
     * Get character at position
     */
    getChar(grid, x, y) {
        x = Math.round(x);
        y = Math.round(y);
        if (y >= 0 && y < this.GRID_HEIGHT && x >= 0 && x < this.GRID_WIDTH) {
            const cell = grid[y][x];
            if (typeof cell === 'string') return cell;
            if (cell && cell.char) return cell.char;
        }
        return ' ';
    }

    /**
     * Draw a line using Bresenham's algorithm
     */
    drawLine(grid, x1, y1, x2, y2, char) {
        x1 = Math.round(x1); y1 = Math.round(y1);
        x2 = Math.round(x2); y2 = Math.round(y2);

        const dx = Math.abs(x2 - x1);
        const dy = Math.abs(y2 - y1);
        const sx = x1 < x2 ? 1 : -1;
        const sy = y1 < y2 ? 1 : -1;
        let err = dx - dy;

        let x = x1, y = y1;

        while (true) {
            this.setChar(grid, x, y, char);
            if (x === x2 && y === y2) break;

            const e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    }

    /**
     * Fill a circle with optional shading
     */
    fillCircle(grid, centerX, centerY, radius, char, shaded = false, lightDir = 'top') {
        const lightOffsets = {
            top: { x: 0, y: -radius },
            topLeft: { x: -radius, y: -radius },
            topRight: { x: radius, y: -radius },
            front: { x: 0, y: 0 }
        };
        const light = lightOffsets[lightDir] || lightOffsets.front;

        for (let y = -radius; y <= radius; y++) {
            for (let x = -radius; x <= radius; x++) {
                if (x * x + y * y <= radius * radius) {
                    if (shaded) {
                        const intensity = this.calculateShading(
                            centerX + x, centerY + y,
                            centerX + light.x, centerY + light.y,
                            centerX, centerY, radius
                        );
                        const shadeChar = this.getShadeChar(intensity, 'SKIN');
                        this.setChar(grid, centerX + x, centerY + y, shadeChar);
                    } else {
                        this.setChar(grid, centerX + x, centerY + y, char);
                    }
                }
            }
        }
    }

    /**
     * Fill an ellipse with optional shading
     */
    fillEllipse(grid, centerX, centerY, radiusX, radiusY, char, shaded = false) {
        for (let y = -radiusY; y <= radiusY; y++) {
            for (let x = -radiusX; x <= radiusX; x++) {
                if ((x * x) / (radiusX * radiusX) + (y * y) / (radiusY * radiusY) <= 1) {
                    if (shaded) {
                        // Edge darkening
                        const edgeDist = Math.sqrt(
                            (x * x) / (radiusX * radiusX) + (y * y) / (radiusY * radiusY)
                        );
                        const intensity = 1 - edgeDist * 0.6;
                        const shadeChar = this.getShadeChar(intensity, 'SKIN');
                        this.setChar(grid, centerX + x, centerY + y, shadeChar);
                    } else {
                        this.setChar(grid, centerX + x, centerY + y, char);
                    }
                }
            }
        }
    }

    /**
     * Draw a quadratic bezier curve
     */
    drawCurve(grid, x0, y0, x1, y1, x2, y2, char) {
        for (let t = 0; t <= 1; t += 0.03) {
            const x = Math.round((1 - t) * (1 - t) * x0 + 2 * (1 - t) * t * x1 + t * t * x2);
            const y = Math.round((1 - t) * (1 - t) * y0 + 2 * (1 - t) * t * y1 + t * t * y2);
            this.setChar(grid, x, y, char);
        }
    }

    /**
     * Draw face outline with proper contour shading
     */
    drawFaceContour(grid, bodyMod = 0, lightDir = 'front') {
        const { CENTER, TOP_OF_HEAD, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT, CHEEKBONE } = this.PROPORTIONS;

        const left = FACE_LEFT - bodyMod;
        const right = FACE_RIGHT + bodyMod;
        const width = right - left;

        // Draw left side with shading
        for (let y = HAIRLINE; y <= CHIN; y++) {
            const progress = (y - HAIRLINE) / (CHIN - HAIRLINE);

            // Face curve - widest at cheekbones
            let xOffset = 0;
            if (y < CHEEKBONE) {
                // Upper face - slight curve out
                xOffset = Math.sin(progress * Math.PI * 0.5) * 2;
            } else {
                // Lower face - curve in toward chin
                const chinProgress = (y - CHEEKBONE) / (CHIN - CHEEKBONE);
                xOffset = 2 - chinProgress * 4;
            }

            const leftX = left + xOffset;
            const rightX = right - xOffset;

            // Edge characters
            this.setChar(grid, leftX, y, '(');
            this.setChar(grid, rightX, y, ')');

            // Contour shading near edges
            if (lightDir === 'topLeft' || lightDir === 'front') {
                this.setChar(grid, leftX + 1, y, '░');
            } else {
                this.setChar(grid, leftX + 1, y, '▒');
            }

            if (lightDir === 'topRight' || lightDir === 'front') {
                this.setChar(grid, rightX - 1, y, '░');
            } else {
                this.setChar(grid, rightX - 1, y, '▒');
            }
        }

        // Chin curve
        const chinWidth = 6 + bodyMod;
        for (let x = CENTER - chinWidth; x <= CENTER + chinWidth; x++) {
            const dx = Math.abs(x - CENTER);
            const y = CHIN + Math.round((1 - dx / chinWidth) * 2);
            this.setChar(grid, x, y, '_');
        }
    }

    /**
     * Convert grid to string
     */
    gridToString(grid) {
        return grid.map(row =>
            row.map(cell => {
                if (typeof cell === 'string') return cell;
                if (cell && typeof cell === 'object' && cell.char) return cell.char;
                return ' ';
            }).join('')
        ).join('\n');
    }
}
