import WbJoint from './WbJoint.js';
import WbVector3 from './utils/WbVector3.js';
import WbVector4 from './utils/WbVector4.js';
import WbQuaternion from './utils/WbQuaternion.js';
import {isZeroAngle} from './utils/math_utilities.js';

export default class WbHingeJoint extends WbJoint {
  #device;
  #position;
  constructor(id) {
    super(id);
    this.#device = [];
  }

  get device() {
    return this.#device;
  }

  set device(device) {
    this.#device = device;
  }

  get position() {
    return this.#position;
  }

  set position(newPosition) {
    if (this.#position === newPosition)
      return;

    this.#position = newPosition;
    if (typeof this.jointParameters === 'undefined')
      this._updatePosition();
  }

  preFinalize() {
    super.preFinalize();
    this.#device.forEach(child => child.preFinalize());
  }

  postFinalize() {
    super.postFinalize();

    this.#device.forEach(child => child.postFinalize());
  }

  delete() {
    let index = this.#device.length - 1;
    while (index >= 0) {
      this.#device[index].delete();
      --index;
    }

    super.delete();
  }

  _updatePosition() {
    if (typeof this.endPoint !== 'undefined')
      this.#updatePosition(typeof this.jointParameters !== 'undefined' ? this.jointParameters.position : this.#position);
  }

  #updatePosition(position) {
    // called after an artificial move
    this.#position = position;
    let rotation = new WbVector4();
    const translation = this.#computeEndPointSolidPositionFromParameters(rotation);
    if (!translation.almostEquals(this.endPoint.translation) || !rotation.almostEquals(this.endPoint.rotation)) {
      this.endPoint.translation = translation;
      this.endPoint.rotation = rotation;
    }
  }

  #computeEndPointSolidPositionFromParameters(rotation) {
    const axis = this.axis().normalized();
    const q = new WbQuaternion();
    q.fromAxisAngle(axis.x, axis.y, axis.z, this.#position);
    const iq = this._endPointZeroRotation.toQuaternion();
    const qp = q.mul(iq);
    if (qp.w !== 1)
      qp.normalize();

    rotation.fromQuaternion(qp);
    if (rotation.w === 0)
      rotation = new WbVector4(axis.x, axis.y, axis.z, 0);
    const a = this.anchor();
    return q.mulByVec3(this._endPointZeroTranslation.sub(a)).add(a);
  }

  _updateEndPointZeroTranslationAndRotation() {
    if (typeof this.endPoint === 'undefined')
      return;

    const ir = this.endPoint.rotation;
    const it = this.endPoint.translation;

    let qMinus;
    const angle = this.#position;
    if (isZeroAngle(angle)) {
      // In case of a zero angle, the quaternion axis is undefined, so we keep track of the original one
      this._endPointZeroRotation = ir;
      qMinus = new WbQuaternion();
    } else {
      const axis = this.axis().normalized();
      qMinus = new WbQuaternion();
      qMinus.fromAxisAngle(axis.x, axis.y, axis.z, -angle);
      const q = ir.toQuaternion();
      let qNormalized = qMinus.mul(q);

      if (qNormalized.w !== 1)
        qNormalized.normalize();
      this._endPointZeroRotation = new WbVector4(qNormalized.x, qNormalized.y, qNormalized.z, qNormalized.w);
      if (this._endPointZeroRotation.w === 0)
        this._endPointZeroRotation = new WbVector4(axis.x, axis.y, axis.z, 0.0);
    }
    const anchor = this.anchor();
    this._endPointZeroTranslation = qMinus.mulByVec3(it.sub(anchor)).add(anchor);
  }

  axis() {
    return typeof this.jointParameters !== 'undefined' ? this.jointParameters.axis : WbHingeJoint.DEFAULT_AXIS;
  }

  anchor() {
    return typeof this.jointParameters !== 'undefined' ? this.jointParameters.anchor : new WbVector3(0, 0, 0);
  }
}

WbHingeJoint.DEFAULT_AXIS = new WbVector3(1.0, 0.0, 0.0);
