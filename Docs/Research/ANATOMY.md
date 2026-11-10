# Anatomy: a stylised 159 cm android woman

Aim (Tema): **proper anatomy**, read as a mannequin-white sculpted shell. Accurate in structure, simplified in surface. Numbers are drawing conventions, not rules. Check against our mesh with a front/side capture before trusting any of them.

## Proportions

- **Heads:** our direction asks for about 8.5 heads (long legs, vertical like the columns). A realistic adult woman is about 7.5. At 159 cm, one head is about 18.7 cm. Keep the head small and the legs long, but move the crotch no higher than about 50% of height or she reads as a doll.
- **Landmarks, from the top (head = 1 unit):** chin 1, nipple line about 2, navel about 3, crotch about 4 in a 8-head figure, knee about 5.5, floor 8. Use them as a check on the sliders in the Body page.
- **Female vs male, structurally:** shoulders narrower than hips at the widest, a longer torso-to-leg ratio in the waist, a higher waist-to-hip curve, a wider pelvis with the hip joint set further apart, and thighs that angle in toward the knees (the Q-angle, so knees sit closer than hips).
- **Shoulder width:** about 2 head-widths across the deltoids for a slim figure. Keep the clavicles slightly upturned, not level.
- **Reference:** [Proko's figure drawing guides](https://www.proko.com/) and Andrew Loomis, *Figure Drawing for All It's Worth*, show landmarks that survive stylisation (not link-verified).

## Surface landmarks that sell anatomy

The eye reads these. If they are missing, the figure reads as plastic. If they are over-detailed, it reads as nude art.
1. **Clavicles and the sternal notch** (the dip at the base of the neck).
2. **Acromion and deltoid** (the shoulder cap), which turns the arm into the torso.
3. **Rib cage as an egg**, narrower than the hips, with a visible waist between ribs and pelvis.
4. **ASIS** (the front hip points) and the **sacral dimples** at the back. Together they say "pelvis".
5. **Knee and ankle** knobs, a defined **Achilles** and calf shape.
6. **The spine as an S-curve**, with a slight lumbar arch and a pelvic tilt forward. This is what makes contrapposto work.
7. **Abs:** our default is soft six-pack lines (AbsLines/AbsPack 0.73). In dust light they read as soft lines, which is right. Do not deepen them.

## Joints and how they deform

Skinning rules of thumb. Our skin uses mannequin weights plus shape-key correctives, not twist-bone driven corrections (see [RIG.md](RIG.md)).

| Joint | Problem | Fix |
|---|---|---|
| **Shoulder (arm raise)** | Armpit folds into a crease, shoulder stacks into a shelf, the chest stays flat. | Our `ArmUp_l/r` morphs (`Tools/blender_correctives.py`), faded in by arm angle. Check any new arm pose above about 90 degrees. |
| **Forearm twist** | "Candy-wrapper" pinching at the wrist. | Spread the twist over lowerarm twist bones (about 50% to 100% down the forearm). |
| **Upper arm twist** | Same at the elbow and shoulder. | Same, with the upper-arm twist bones. |
| **Elbow** | Inner crease pinches, outer elbow caves in. | Keep weights soft over about 4 cm of overlap. Bend past 120 degrees is a skin-folding case, so test it. |
| **Hip (thigh flexion)** | Crotch crease collapses, the glute pulls forward. Deep sit and kneel make this worst. | Corrective morph on thigh flexion beyond about 90 degrees. Pose wheel floor poses hit this. |
| **Knee** | Back-of-knee volume loss, front volume pop. | Slight bulge corrective at the back of the knee past 100 degrees. |
| **Spine** | Waist pinches when twisted or side-bent. | Spread twist over spine_01 to spine_05, never one bone. Our catwalk layer counter-rotates the chest. |
| **Neck** | Trapezius collapses on tilt. | Keep the trapezius weight on clavicle and spine_05, not the neck. |
| **Ankle / toe** | Skin tears when the foot tilts or rises on the ball. | Soft weights on foot and ball. We already limit ankles to real joint ranges in poses. |

## Volume preservation

Where volume has to be kept, in order of visibility:
1. **Hip and thigh in deep flexion** (seated, kneeling, lying). The thigh should bulge, not flatten.
2. **Upper arm and shoulder when raised** (pin-up arms over the head).
3. **Calf behind the knee** when the leg is folded.
4. **Waist when twisted.** Stretch is fine, but do not lose width.
5. **Bust on the chest wall.** Stays attached and follows the pectoral. See the next section.

Stylised bodies can cheat: a small constant volume offset (0.5 to 1 cm) along the normal at the thigh and glute in deep flexion hides most collapse.

## Bust, hip and shoulder pitfalls (skinning and cloth)

- **Bust:** weights should come from the chest (spine_04, spine_05, clavicle), not the arm. A heavy arm weight drags the bust sideways whenever the arm swings. Our jiggle is spring-driven morphs (BustUp, BustSway) fed by the bone velocity, so the base weights must be quiet. Standing damping is higher (`StandingDamping`) so idle stays calm. Keep the sway small: **calm and proud** wants a barely-moving chest.
- **Bust and outfit:** the outfit takes all the skin's weights (up to 8 per vertex) and **every shape key** from the vertices it was cut from, or cloth sinks into the body at a joint (`Docs/CLOTHES.md`). Any new morph on the skin must also go on the outfit.
- **Hip:** the hip carries up to 7 weights. Cutting to 4 let skin show in slivers. Keep the limit at 8 on cloth.
- **Glute:** each glute half rides its own thigh. A deep hip bend can drag it into the opposite side, so check the crotch.
- **Shoulder:** cloth with a single clavicle weight rides up when the arm rises. Blend to the upper arm.
- **Garter straps:** skinned ribbons can cut into the thigh in a deep hip bend (known limit).
- **No simulation:** clothes are skinned, not simulated. Plan wind and hem movement as bone or morph drivers, not as cloth.

## Android specifics

- **Two layers:** machine inside, opaque mannequin-white skin outside. Humanlike shape, but the surface is a clean sculpt with no pores and no joint seams.
- **Skin joints are invisible**, so the anatomy has to carry the pose. A stiff joint cannot hide behind a seam.
- **Mannequin detail:** medium bust, no nipples, a soft contour. Stay abstract.
- **Android moves are "slightly too precise":** clean stops, head-first turns (see [ANIMATION.md](ANIMATION.md)). Anatomy still has to bend like a person, with weight and counterbalance.

## Checks before accepting a pose or clip

1. Front and side capture at 4K. Look at the armpit, crotch, back of the knee and waist.
2. Rotate the camera 90 degrees: does any limb flatten or the chest pass through the arm?
3. Is the silhouette readable as a flat shape (head, shoulders, waist, hips, legs)?
4. Does the body keep one centre of mass over the supporting foot?
