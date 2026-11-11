const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const [, , inputDirectory, outputDirectory] = process.argv;
assert(inputDirectory && outputDirectory, 'Usage: node bake.cjs <input-directory> <output-directory>');
const input = path.resolve(inputDirectory);
const output = path.resolve(outputDirectory);
const read = (name) => JSON.parse(fs.readFileSync(path.join(input, name), 'utf8').replace(/^\uFEFF/, ''));

const { rig } = read('source-rigs.json');
const locomotion = read('baked-motion.json');
const downed = read('downed-motion.json');
const { bones } = locomotion;
const { clips } = read('source-manifest.json');
const animationProperties = read('animation-properties.json');
const byName = new Map(animationProperties.map((clip) => [clip.name, clip]));
const byPath = new Map(animationProperties.map((clip) => [clip.path, clip]));
const fps = 120;
const transformBytes = 40;
const unitScale = [1, 1, 1];
const manifest = { bones: bones.map((bone) => bone.name), clips: [] };
fs.mkdirSync(output, { recursive: true });

function multiply([ax, ay, az, aw], [bx, by, bz, bw]) {
    return [
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ];
}
const inverse = ([x, y, z, w]) => [-x, -y, -z, w];
const assetPath = (ref) => ref.ObjectPath.replace(/\.\d+$/, '').replace('QueensDomain/Content', '/Game') +
    '.' + ref.ObjectName.match(/'([^']+)'/)[1];

function writeFrames(name, frames) {
    const file = path.join(output, name + '.bin');
    const buffer = Buffer.alloc(frames.length * bones.length * transformBytes);
    let offset = 0;
    for (const frame of frames) {
        assert.equal(frame.length, bones.length);
        for (const transform of frame) {
            for (const number of [...transform.p, ...transform.q, ...transform.s]) {
                assert(Number.isFinite(number));
                offset = buffer.writeFloatLE(number, offset);
            }
        }
    }
    assert.equal(offset, buffer.length);
    fs.writeFileSync(file, buffer);
    return file.replaceAll('\\', '/');
}

function retarget(clip) {
    const source = fs.readFileSync(path.join(input, 'source', clip.name + '.bin'));
    const frameSize = rig.mapping.length * transformBytes;
    const frameCount = Math.round(clip.duration * fps) + 1;
    assert.equal(source.length, frameCount * frameSize, clip.name);

    return Array.from({ length: frameCount }, (_, frame) => {
        const mapped = new Map();
        for (const [index, mapping] of rig.mapping.entries()) {
            const target = mapping.target - 1;
            const offset = frame * frameSize + index * transformBytes + 12;
            const rotation = Array.from({ length: 4 }, (_, axis) => source.readFloatLE(offset + axis * 4));
            const bindDelta = multiply(inverse(mapping.bind), rig.target[target].cq);
            mapped.set(target, multiply(rotation, bindDelta));
        }
        const componentRotations = [];
        return bones.map((bone, index) => {
            const parent = componentRotations[bone.parent];
            const component = mapped.get(index) ?? (parent ? multiply(parent, bone.q) : bone.q);
            componentRotations.push(component);
            const local = parent ? multiply(inverse(parent), component) : component;
            const length = Math.hypot(...local);
            return { p: bone.p, q: local.map((value) => value / length), s: unitScale };
        });
    });
}

function isAdditive(clip) {
    if (clip.type !== 'AnimMontage') return clip.additive !== 'AAT_None';
    const tracks = byName.get(clip.name).props.SlotAnimTracks;
    const types = new Set(tracks.flatMap((slot) => slot.AnimTrack.AnimSegments.map(
        (segment) => byPath.get(assetPath(segment.AnimReference))?.additive || 'AAT_None',
    )));
    assert.equal(types.size, 1, 'Mixed additive tracks: ' + clip.name);
    return ![...types][0].includes('None');
}

function emit(clip, frames) {
    const file = writeFrames(clip.name, frames);
    const duration = (frames.length - 1) / clip.fps;
    manifest.clips.push({ ...clip, frames: frames.length, duration, file });
}

for (const clip of clips) {
    emit({ ...clip, fps, additive: isAdditive(clip), sourceDuration: clip.duration }, retarget(clip));
}
for (const clip of [...locomotion.clips.filter((clip) => clip.kind === 'move'), ...downed.clips]) {
    emit(
        { name: clip.name, fps: clip.fps, type: 'Body', additive: false, slots: [], sections: [] },
        clip.frames.map((frame) => frame.localPose),
    );
}
manifest.referenceFile = writeFrames('original-reference', [
    bones.map((bone) => ({ p: bone.p, q: bone.q, s: unitScale })),
]);
fs.writeFileSync(path.join(output, 'baked-manifest.json'), JSON.stringify(manifest, null, 2));
